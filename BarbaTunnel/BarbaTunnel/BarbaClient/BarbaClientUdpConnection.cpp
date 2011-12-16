#include "StdAfx.h"
#include "BarbaClientUdpConnection.h"


BarbaClientUdpConnection::BarbaClientUdpConnection(LPCTSTR connectionName, BarbaKey* key)
	: BarbaClientConnection(connectionName, key)
{
}

BarbaModeEnum BarbaClientUdpConnection::GetMode()
{
	return BarbaModeUdpTunnel;
}


BarbaClientUdpConnection::~BarbaClientUdpConnection(void)
{
}

bool BarbaClientUdpConnection::ExtractUdpBarbaPacket(PacketHelper* barbaPacket, BYTE* orgPacketBuffer)
{
	CryptPacket(barbaPacket);
	PacketHelper orgPacket(orgPacketBuffer, 0);
	orgPacket.SetEthHeader(barbaPacket->ethHeader);
	return orgPacket.SetIpPacket((iphdr_ptr)barbaPacket->GetUdpPayload()) && orgPacket.IsValidChecksum();
}

bool BarbaClientUdpConnection::CreateUdpBarbaPacket(PacketHelper* packet, BYTE* barbaPacketBuffer)
{
	packet->RecalculateChecksum();

	PacketHelper barbaPacket(barbaPacketBuffer, IPPROTO_UDP);
	barbaPacket.SetEthHeader(packet->ethHeader);
	barbaPacket.ipHeader->ip_ttl = packet->ipHeader->ip_ttl;
	barbaPacket.ipHeader->ip_v = packet->ipHeader->ip_v;
	barbaPacket.ipHeader->ip_id = 56;
	barbaPacket.ipHeader->ip_off = packet->ipHeader->ip_off;
	barbaPacket.SetSrcIp(packet->GetSrcIp());
	barbaPacket.SetDesIp(this->Config->ServerIp);
	barbaPacket.SetSrcPort(this->ClientPort);
	barbaPacket.SetDesPort(this->TunnelPort);
	barbaPacket.SetUdpPayload((BYTE*)packet->ipHeader, packet->GetIpLen());
	CryptPacket(&barbaPacket);
	return true;
}

bool BarbaClientUdpConnection::ProcessPacket(INTERMEDIATE_BUFFER* packetBuffer)
{
	bool send = packetBuffer->m_dwDeviceFlags==PACKET_FLAG_ON_SEND;
	PacketHelper packet(packetBuffer->m_IBuffer);

	if (send)
	{
		//Create Barba packet
		BYTE barbaPacket[MAX_ETHER_FRAME];
		if (!CreateUdpBarbaPacket(&packet, barbaPacket))
			return false;
			
		//replace current packet with barba packet
		packet.SetEthPacket((ether_header_ptr)barbaPacket);
		packet.RecalculateChecksum();
		packetBuffer->m_Length = packet.GetPacketLen();
		SetWorkingState(packetBuffer->m_Length, send);
		return true;
	}
	else
	{
		//extract Barba packet
		BYTE orgPacketBuffer[MAX_ETHER_FRAME];
		if (!ExtractUdpBarbaPacket(&packet, orgPacketBuffer))
			return false;
		PacketHelper orgPacket(orgPacketBuffer);
	
		packet.SetEthPacket(orgPacket.ethHeader);
		packet.RecalculateChecksum();
		packetBuffer->m_Length = packet.GetPacketLen();
		SetWorkingState(packetBuffer->m_Length, send);
		return true;
	}
}