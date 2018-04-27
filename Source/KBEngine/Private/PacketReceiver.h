#pragma once
#include "KBEnginePrivatePCH.h"
#include "MessageReader.h"

namespace KBEngine
{
	class MessageReader;
	class NetworkInterface;

	class PacketReceiver : public FRunnable
	{

	public:
		PacketReceiver(NetworkInterface* networkInterface, uint32 buffLength = 65535);
		~PacketReceiver();

		void Process(MessageReader& messageReader);
		void StartBackgroundRecv();
		void WillClose() { willClose_ = true; }

	public:
		// for FRunnable
		virtual uint32 Run() override;
		void DoThreadedWork();


	private:
		// ����������socket�У���������ӿڿ��ܻᵼ�¿������ⲿ�ǲ������ɱ���
		void StopBackgroundRecv();

		// ���߳��е��ã������д�Ļ������ռ�
		uint32 FreeWriteSpace();

		// ���߳��е��ã���ʼ��Socket�ж�ȡ����
		void BackgroundRecv();

	private:
		NetworkInterface* networkInterface_ = NULL;

		uint8* buffer_;
		uint32 bufferLength_ = 0;

		// socket�򻺳���д����ʼλ��
		uint32 wpos_ = 0;

		// ���̶߳�ȡ���ݵ���ʼλ��
		uint32 rpos_ = 0;

		FRunnableThread* thread_ = nullptr;
		bool breakThread_ = false;

		// ��NetworkInterface�ر�����ʱ֪ͨ��
		// �Ա����������ر�����ʱҲ����������Ϣ
		bool willClose_ = false;
	};

}