#include "PacketSender.h"
#include "KBEnginePrivatePCH.h"
#include "GenericPlatformProcess.h"
#include "NetworkInterface.h"


namespace KBEngine
{
	PacketSender::PacketSender(NetworkInterface* networkInterface, uint32 buffLength) :
		buffer_(new uint8[buffLength]),
		bufferLength_(buffLength),
		networkInterface_(networkInterface)
	{
		InitPipe();
	}

	PacketSender::~PacketSender()
	{
		KBE_DEBUG(TEXT("PacketSender::~PacketSender()"));
		StopBackgroundSend();

		if (buffer_)
			delete buffer_;

		ClosePipe();
	}

	bool PacketSender::Send(uint8* datas, uint32 length)
	{
		KBE_ASSERT(length > 0);

		uint32 t_spos = spos_;

		//д�±���ڵ��ڷ����±꣬˵����û������߸ոշ��꣬���һ�û�лػ�д����д��β������ͷ��д
		if (wpos_ >= t_spos)
		{
			uint32 space1 = bufferLength_ - wpos_;
			uint32 space2 = t_spos - 1;		//д�±�׷�Ϸ����±�ʱ����1����λ�������ж��Ƿ��껹��д��
			if (length > space1 + space2)
			{
				KBE_ERROR(TEXT("PacketSender::Send() : no space, Please adjust 'SEND_BUFFER_MAX'!data(%d) > space(%d), wpos=%u, spos=%u"), length, space1 + space2, wpos_, t_spos);
				return false;
			}

			if (length <= space1)
			{
				memcpy(&(buffer_[wpos_]), datas, length);
				wpos_ += length;
			}
			else
			{
				if (space1 > 0)
				{
					memcpy(&(buffer_[wpos_]), datas, space1);
				}
				memcpy(&(buffer_[0]), datas + space1, length - space1);
				wpos_ = length - space1;
			}
		}

		//�����±����д�±꣬˵����û���겢���Ѿ��ػ�д����д�����±���д�±�֮���λ
		else
		{
			if (length > t_spos - wpos_ - 1)
			{
				KBE_ERROR(TEXT("PacketSender::Send() : no space, Please adjust 'SEND_BUFFER_MAX'!data(%d) > space(%d), wpos=%u, spos=%u"), length, t_spos - wpos_ - 1, wpos_, t_spos);
				return false;
			}

			memcpy(&(buffer_[wpos_]), datas, length);
			wpos_ += length;
		}

		KBE_DEBUG(TEXT("PacketSender::Send() : data(%d), wpos=%u, spos=%u"), length, wpos_, t_spos);

		WritePipe();

		return true;
	}

	void PacketSender::StopBackgroundSend()
	{
		if (thread_)
		{
			breakThread_ = true;
			WritePipe(); // ��������

			// �����ȴ��߳̽���
			thread_->WaitForCompletion();
			delete thread_;
			thread_ = nullptr;
		}
	}

	void PacketSender::StartBackgroundSend()
	{
		KBE_ASSERT(!thread_);
		breakThread_ = false;
		thread_ = FRunnableThread::Create(this, *FString::Printf(TEXT("KBEnginePacketSender:%p"), this));
	}

	uint32 PacketSender::Run()
	{
		DoThreadedWork();
		return 0;
	}

	void PacketSender::DoThreadedWork()
	{
		while (true)
		{
			FString data = ReadPipe();
			if (data.Len() == 0)
			{
				KBE_ERROR(TEXT("PacketSender::DoThreadedWork: pipe closed!"));
				break;
			}
			//KBE_DEBUG(TEXT("PacketSender::DoThreadedWork(), read pipe data '%s'"), *data);

			if (breakThread_)
				break;

			if (!networkInterface_ || !networkInterface_->Valid())
				break;
			BackgroundSend();
		}
		sending_ = false;
		breakThread_ = false;
	}

	uint32 PacketSender::SendSize() const
	{
		uint32 t_wpos = wpos_;
		if (t_wpos >= spos_)
		{
			return t_wpos - spos_;
		}
		return bufferLength_ - spos_ + t_wpos;
	}

	void PacketSender::BackgroundSend()
	{
		uint32 sendSize = SendSize();
		if (sendSize == 0)
		{
			KBE_WARNING(TEXT("PacketSender::BackgroundSend: require send data, but no data to send!!!"));
			return;
		}

		uint32 tailSize = bufferLength_ - spos_;
		int32 bytesSent = 0;

		if (sendSize <= tailSize)
		{
			RealBackgroundSend(sendSize, bytesSent);
			return;
		}

		uint32 remainSize = sendSize;
		if (tailSize > 0)
		{
			RealBackgroundSend(tailSize, bytesSent);
			if (tailSize != bytesSent)
				return;
			remainSize = sendSize - tailSize;
		}
		spos_ = 0;	//���÷���λ��
		RealBackgroundSend(remainSize, bytesSent);
	}

	void PacketSender::RealBackgroundSend(uint32 sendSize, int32 &bytesSent)
	{
		networkInterface_->Socket()->Send(&(buffer_[spos_]), sendSize, bytesSent);

		if (bytesSent == -1)
		{
#if PLATFORM_WINDOWS
			int id = GetLastError();
			FString errStr;

			switch (id)
			{
			case WSANOTINITIALISED: errStr = TEXT("PacketReceiver::BackgroundRecv: not initialized"); break;
			case WSASYSNOTREADY:    errStr = TEXT("PacketReceiver::BackgroundRecv: sub sys not ready"); break;
			case WSAHOST_NOT_FOUND: errStr = TEXT("PacketReceiver::BackgroundRecv: name server not found"); break;
			case WSATRY_AGAIN:      errStr = TEXT("PacketReceiver::BackgroundRecv: server fail"); break;
			case WSANO_RECOVERY:    errStr = TEXT("PacketReceiver::BackgroundRecv: no recovery"); break;
			case WSAEINPROGRESS:    errStr = TEXT("PacketReceiver::BackgroundRecv: socket blocked by other prog"); break;
			case WSANO_DATA:        errStr = TEXT("PacketReceiver::BackgroundRecv: no data record"); break;
			case WSAEINTR:          errStr = TEXT("PacketReceiver::BackgroundRecv: blocking call canciled"); break;
			case WSAEPROCLIM:       errStr = TEXT("PacketReceiver::BackgroundRecv: limit exceeded"); break;
			case WSAEFAULT:         errStr = TEXT("PacketReceiver::BackgroundRecv: lpWSAData in startup not valid."); break;
			case WSAECONNABORTED:   errStr = TEXT("PacketReceiver::BackgroundRecv: connect aborted!"); break;
			default:				errStr = FString::Printf(TEXT("PacketReceiver::OnBackgroundRecv: unknown error id: %d"), id); break;
			};
			if (!willClose_)
				KBE_ERROR(TEXT("%s"), *errStr);
#endif				

			breakThread_ = true;
			if (!willClose_)
				networkInterface_->WillClose();
			return;
		}

		if (bytesSent > 0)
		{
			spos_ += bytesSent;
		}
	}

	FString PacketSender::ReadPipe()
	{
		FString Output;

#if PLATFORM_WINDOWS
		UTF8CHAR Buffer[255];
		uint32 BytesRead = 0;
		if (::ReadFile(readPipe_, Buffer, 254, (::DWORD*)&BytesRead, NULL))
		{
			if (BytesRead > 0)
			{
				Buffer[BytesRead] = '\0';
				Output += FUTF8ToTCHAR((const ANSICHAR*)Buffer).Get();
			}
		}
#else
		KBE_ASSERT(false);
#endif
		return Output;
	}

	void PacketSender::WritePipe()
	{
		FPlatformProcess::WritePipe(writePipe_, TEXT("1"));
	}

	void PacketSender::InitPipe()
	{
		FPlatformProcess::CreatePipe(readPipe_, writePipe_);
	}

	void PacketSender::ClosePipe()
	{
		if (readPipe_ || writePipe_)
		{
			FPlatformProcess::ClosePipe(readPipe_, writePipe_);
			readPipe_ = writePipe_ = nullptr;
		}
	}
}

