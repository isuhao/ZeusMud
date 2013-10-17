#ifndef __SESSION_H__
#define __SESSION_H__

#include <common.h>
#include <network_common.h>
#include <protobuf.h>
#include <packet.h>
#include <tcp_connection.h>
#include "opcodes.h"
#include "game_database_session.h"
#include "game_util.h"

class Session
{
public:
    Session(const uint64& session_id)
        : _sessionId(session_id),
        _connection(nullptr)
    {
    }

    ~Session()
    {
    }

public:
    void set_connection_ptr(const TcpConnectionPtr& connection)
    {
        _connection = connection;
    }

    uint64_t session_id() const
    {
        return _sessionId;
    }

    template <typename T> void send_message(uint32 opcode, const T& message)
    {
        if (_connection != nullptr)
        {
            size_t messageSize = message.ByteSize();
            size_t packetSize = ServerPacket::HEADER_LENGTH + messageSize;

            ByteBuffer packet_buffer;
            packet_buffer << packetSize;
            packet_buffer << opcode;
            
            byte* message_data = new byte[messageSize];
            message.SerializeToArray(message_data, messageSize);
            packet_buffer.append(message_data, messageSize);

            _connection->writeAsync(packet_buffer.buffer(), packet_buffer.size());

            SAFE_DELETE_ARR(message_data);
        }
    }

public:
    void user_login_handler(const NetworkMessage& message)
    {
        Protocol::C2SLoginReq request;
        message.parse(request);

        printf("[User Login] -> (Username='%s', Password='%s')", request.email().c_str(), request.password().c_str());

        Protocol::S2CLoginRsp login_response;
        bool exists = GameDatabaseSession::getInstance().checkUserExists(request.email());
        login_response.set_login_result(exists);
        login_response.set_failed_reason(exists == true ? "" : "用户不存在。");

        send_message<Protocol::S2CLoginRsp>(Opcodes::S2CLoginRsp, login_response);
    }

    void user_register_handler(const NetworkMessage& message)
    {
        Protocol::C2SRegisterReq request;
        message.parse(request);

        printf("[User Register] -> (Username='%s', Nickname='%s')", request.email().c_str(), request.nickname().c_str());

        Protocol::S2CRegisterRsp register_response;
        bool exists = GameDatabaseSession::getInstance().checkUserExists(request.email());
        if (exists == true)
        {
            register_response.set_register_result(false);
            register_response.set_failed_reason("注册失败，该用户已存在，请换个拉风点的邮箱帐号。");
        }
        else
        {
            GameDatabaseSession::getInstance().insertNewUserRecord(
                GameUtil::getInstance().toUniqueId(request.email()),
                request.email(),
                request.password(),
                (uint8)request.gender(),
                request.nickname(),
                _connection->getPeerAddress().host(),
                Poco::Timestamp().epochTime());
            
            register_response.set_register_result(true);
            register_response.set_failed_reason("");
        }

        send_message<Protocol::S2CRegisterRsp>(Opcodes::S2CRegisterRsp, register_response);
    }

private:
    uint64 _sessionId;
    TcpConnectionPtr _connection;
};

#endif