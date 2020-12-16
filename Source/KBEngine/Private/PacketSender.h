#pragma once

#include "KBEDebug.h"

namespace KBEngine
{
	class NetworkInterface;

	class PacketSender : public FRunnable
	{
	public:
		PacketSender(NetworkInterface* networkInterface, uint32 buffLength = 65535);
		~PacketSender();

		bool Send(uint8* datas, uint32 length);

		void StartBackgroundSend();
		void WillClose() { willClose_ = true; }

	public:
		// for FRunnable
		virtual uint32 Run() override;
		void DoThreadedWork();

	private:
		// �����п���������socket�У���������ӿڿ��ܻᵼ�¿������ⲿ�ǲ������ɱ���
		void StopBackgroundSend();

		uint32 SendSize() const;
		void BackgroundSend();
		void  RealBackgroundSend(uint32 sendSize, int32 &bytesSent);
		FString ReadPipe();
		void WritePipe();
		void InitPipe();
		void ClosePipe();

	private:
		uint8* buffer_;
		uint32	bufferLength_ = 0;

		uint32 wpos_ = 0;				// д�������λ��
		uint32 spos_ = 0;				// ������ϵ�����λ��
		bool sending_ = false;

		NetworkInterface* networkInterface_ = NULL;

		FRunnableThread* thread_ = nullptr;
		bool breakThread_ = false;

		void* readPipe_;
		void* writePipe_;

		// ��NetworkInterface�ر�����ʱ֪ͨ��
		// �Ա����������ر�����ʱҲ����������Ϣ
		bool willClose_ = false;

		template <typename T> T read(uint32 pos) const
		{
			T val = *((T const*)&buffer_[pos]);
			EndianConvert(val);
			return val;
		}

		void hexlike(uint32 startPos, uint32 tSize)
		{
			uint32 j = 1, k = 1;
			char buf[1024];
			std::string fbuffer;
			uint32 endPos = startPos + tSize;

			_snprintf(buf, 1024, "STORAGE_SIZE: endPos=%lu, startPos=%lu.\n", (unsigned long)endPos, (unsigned long)startPos);
			fbuffer += buf;

			uint32 i = 0;
			for (uint32 idx = startPos; idx < endPos; ++idx)
			{
				++i;
				if ((i == (j * 8)) && ((i != (k * 16))))
				{
					if (read<uint8>(idx) < 0x10)
					{
						_snprintf(buf, 1024, "| 0%X ", read<uint8>(idx));
						fbuffer += buf;
					}
					else
					{
						_snprintf(buf, 1024, "| %X ", read<uint8>(idx));
						fbuffer += buf;
					}
					++j;
				}
				else if (i == (k * 16))
				{
					if (read<uint8>(idx) < 0x10)
					{
						_snprintf(buf, 1024, "\n0%X ", read<uint8>(idx));
						fbuffer += buf;
					}
					else
					{
						_snprintf(buf, 1024, "\n%X ", read<uint8>(idx));
						fbuffer += buf;
					}

					++k;
					++j;
				}
				else
				{
					if (read<uint8>(idx) < 0x10)
					{
						_snprintf(buf, 1024, "0%X ", read<uint8>(idx));
						fbuffer += buf;
					}
					else
					{
						_snprintf(buf, 1024, "%X ", read<uint8>(idx));
						fbuffer += buf;
					}
				}
			}

			fbuffer += "\n";

			KBE_ERROR(TEXT("%s"), *FString(fbuffer.c_str()));
		}
	};

}