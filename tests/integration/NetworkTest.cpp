#include <catch2/catch_test_macros.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <iostream>
#include <cstdio>

#include "network/TcpServer.hpp"
#include "network/UdpServer.hpp"
#include "network/SessionManager.hpp"
#include "network/Serializer.hpp"
#include "network/NetworkMessages.hpp"
#include "match/MatchManager.hpp"
#include "ServerConfig.hpp"

using namespace kungfu;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

namespace {

// ============================================================================
// סייען לקוח (TestClient) המדמה שחקן רשת בהתאם לפרוטוקול המעודכן
// ============================================================================
struct TestClient {
    boost::asio::io_context& io;
    tcp::socket tcpSocket;
    udp::socket udpSocket;
    udp::endpoint serverUdpEndpoint;

    std::string username;
    std::string password;
    std::uint64_t sessionToken = 0;
    std::uint64_t matchId = 0;
    std::uint8_t color = 0;

    TestClient(boost::asio::io_context& ioContext, std::uint16_t port, std::string user, std::string pass)
        : io(ioContext),
          tcpSocket(ioContext),
          udpSocket(ioContext, udp::endpoint(udp::v4(), 0)),
          serverUdpEndpoint(boost::asio::ip::make_address("127.0.0.1"), port),
          username(std::move(user)),
          password(std::move(pass)) {}

    // קריאה פסיבית לקבלת פריים TCP בעל טיימאאוט
    bool readTcpFrame(NetworkMessageType& outType, std::vector<std::uint8_t>& outPayload, int timeoutMs = 3000) {
        tcpSocket.non_blocking(true);
        auto start = std::chrono::steady_clock::now();

        std::vector<std::uint8_t> header(kHeaderSize);
        std::size_t headerRead = 0;

        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            boost::system::error_code ec;
            if (headerRead < kHeaderSize) {
                std::size_t n = tcpSocket.read_some(boost::asio::buffer(header.data() + headerRead, kHeaderSize - headerRead), ec);
                if (!ec) headerRead += n;
            }

            if (headerRead == kHeaderSize) {
                std::size_t offset = 0;
                std::uint8_t rawType = 0;
                std::uint32_t payloadLen = 0;
                Serializer::readU8(header, offset, rawType);
                Serializer::readU32(header, offset, payloadLen);

                outType = static_cast<NetworkMessageType>(rawType);
                outPayload.resize(payloadLen);

                std::size_t payloadRead = 0;
                while (payloadRead < payloadLen &&
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start).count() < timeoutMs) {
                    std::size_t n = tcpSocket.read_some(
                        boost::asio::buffer(outPayload.data() + payloadRead, payloadLen - payloadRead), ec);
                    if (!ec) payloadRead += n;
                    else std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                if (payloadRead == payloadLen) {
                    tcpSocket.non_blocking(false);
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        tcpSocket.non_blocking(false);
        return false;
    }

    // קריאה פסיבית לקבלת הודעת UDP בעלת טיימאאוט
    bool receiveUdpFrame(NetworkMessageType& outType, std::vector<std::uint8_t>& outPayload, int timeoutMs = 3000) {
        udpSocket.non_blocking(true);
        auto start = std::chrono::steady_clock::now();
        std::vector<std::uint8_t> buf(2048);
        udp::endpoint senderEp;

        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < timeoutMs) {
            boost::system::error_code ec;
            std::size_t bytesRecvd = udpSocket.receive_from(boost::asio::buffer(buf), senderEp, 0, ec);
            if (!ec && bytesRecvd >= kHeaderSize) {
                std::size_t offset = 0;
                std::uint8_t rawType = 0;
                std::uint32_t payloadLen = 0;
                if (Serializer::readU8(buf, offset, rawType) && Serializer::readU32(buf, offset, payloadLen)) {
                    outType = static_cast<NetworkMessageType>(rawType);
                    if (offset + payloadLen <= bytesRecvd) {
                        outPayload.assign(buf.begin() + offset, buf.begin() + offset + payloadLen);
                        udpSocket.non_blocking(false);
                        return true;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        udpSocket.non_blocking(false);
        return false;
    }

    // 1. התחברות TCP, הרשמה, אימות וחיבור סשן ה-UDP
    bool connectAndAuthenticate(std::uint16_t port) {
        boost::system::error_code ec;
        tcpSocket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port), ec);
        if (ec) return false;

        // שליחת בקשת הרשמה (REGISTER_REQUEST)
        auto regPayload = Serializer::serializeAuthRequest(username, password);
        auto regFrame = Serializer::buildFrame(NetworkMessageType::REGISTER_REQUEST, regPayload);
        boost::asio::write(tcpSocket, boost::asio::buffer(regFrame));

        NetworkMessageType responseType;
        std::vector<std::uint8_t> payload;
        if (!readTcpFrame(responseType, payload) || responseType != NetworkMessageType::REGISTER_RESPONSE) {
            return false;
        }

        // שליחת בקשת התחברות (LOGIN_REQUEST)
        auto loginPayload = Serializer::serializeAuthRequest(username, password);
        auto loginFrame = Serializer::buildFrame(NetworkMessageType::LOGIN_REQUEST, loginPayload);
        boost::asio::write(tcpSocket, boost::asio::buffer(loginFrame));

        if (!readTcpFrame(responseType, payload) || responseType != NetworkMessageType::LOGIN_RESPONSE) {
            return false;
        }

        bool success = false;
        int rating = 0;
        if (!Serializer::deserializeLoginResponse(payload, success, rating, sessionToken) || !success) {
            return false;
        }

        // הצמדת ערוץ ה-UDP סביב ה-sessionToken (SESSION_BIND)
        auto bindPayload = Serializer::serializeSessionBind(sessionToken);
        auto bindFrame = Serializer::buildFrame(NetworkMessageType::SESSION_BIND, bindPayload);
        udpSocket.send_to(boost::asio::buffer(bindFrame), serverUdpEndpoint);

        // קבלת אישור SESSION_BIND_ACK ב-UDP
        if (!receiveUdpFrame(responseType, payload) || responseType != NetworkMessageType::SESSION_BIND_ACK) {
            return false;
        }

        return true;
    }

    // 2. שליחת בקשה להצטרפות לתור שידוך המשחקים (Matchmaking)
    void joinMatchmaking() {
        auto joinFrame = Serializer::buildFrame(NetworkMessageType::JOIN_MATCH_REQUEST, {});
        boost::asio::write(tcpSocket, boost::asio::buffer(joinFrame));
    }

    // 3. האזנה לקבלת הודעת הוספה למשחק (MATCH_FOUND)
    bool waitForMatchFound(int timeoutMs = 4000) {
        NetworkMessageType type;
        std::vector<std::uint8_t> payload;
        if (readTcpFrame(type, payload, timeoutMs) && type == NetworkMessageType::MATCH_FOUND) {
            std::size_t offset = 0;
            std::string opponentUser;
            std::uint32_t opponentElo = 0;
            if (Serializer::readU64(payload, offset, matchId) &&
                Serializer::readU8(payload, offset, color)) {
                return true;
            }
        }
        return false;
    }

    // 4. שליחת מהלך דרך ערוץ ה-UDP
    void sendMove(std::uint64_t requestId, NetworkPosition from, NetworkPosition to) {
        NetworkMovePacket packet{matchId, requestId, color, from, to};
        auto movePayload = Serializer::serializeMovePacket(packet);
        auto moveFrame = Serializer::buildFrame(NetworkMessageType::GAME_MOVE, movePayload);
        udpSocket.send_to(boost::asio::buffer(moveFrame), serverUdpEndpoint);
    }
};

} // namespace

// ============================================================================
// הטסט הראשי
// ============================================================================
TEST_CASE("Asynchronous Network Matchmaking and Move Relay Integration Test", "[network]") {
    const std::string dbPath = "test_kungfu_chess.db";
    std::remove(dbPath.c_str()); // ניקוי בסיס נתונים ישן אם קיים

    boost::asio::io_context serverIo;
    const std::uint16_t testPort = 8088;

    // 1. אתחול מנהלי המערכת והשרתים (TCP & UDP)
    MatchManager matchManager(serverIo);
    SessionManager sessionManager;

    REQUIRE(matchManager.userRepository()->initialize(dbPath) == true);

    TcpServer tcpServer(serverIo, testPort, matchManager, sessionManager);
    UdpServer udpServer(serverIo, testPort, sessionManager);

    // 2. הרצת השרת בטרד נפרד
    std::thread serverThread([&]() {
        serverIo.run();
    });

    // 3. יצירת שני שחקנים עצמאיים
    boost::asio::io_context clientIo;
    TestClient player1(clientIo, testPort, "Alice", "password123");
    TestClient player2(clientIo, testPort, "Bob", "password456");

    // 4. ביצוע תהליך אימות והצמדת ערוץ UDP (Session Binding)
    REQUIRE(player1.connectAndAuthenticate(testPort) == true);
    REQUIRE(player2.connectAndAuthenticate(testPort) == true);

    // 5. הצטרפות למערכת שידוך המשחקים (Matchmaking)
    player1.joinMatchmaking();
    player2.joinMatchmaking();

    // 6. המתנה לקבלת הודעת MATCH_FOUND עבור שני השחקנים
    REQUIRE(player1.waitForMatchFound() == true);
    REQUIRE(player2.waitForMatchFound() == true);

    // אימות שהשחקנים שובצו לאותו משחק בעל מזהה תקף
    REQUIRE(player1.matchId != 0);
    REQUIRE(player1.matchId == player2.matchId);

    // אימות שהשרת הוקצה צבע שונה לכל שחקן (לבן=0, שחור=1)
    REQUIRE(player1.color != player2.color);

    // 7. זיהוי השחקן הלבן והשחקן השחור
    TestClient& whitePlayer = (player1.color == static_cast<std::uint8_t>(PlayerColor::White)) ? player1 : player2;
    TestClient& blackPlayer = (player1.color == static_cast<std::uint8_t>(PlayerColor::White)) ? player2 : player1;

    // 8. שליחת מהלך מועבר מ-White (למשל: רגלי e2 -> e4, כלומר row 6 -> row 4, col 4 -> col 4)
    NetworkPosition fromPos{4, 6}; // col: 4 (e), row: 6
    NetworkPosition toPos{4, 4};   // col: 4 (e), row: 4
    std::uint64_t moveRequestId = 101;

    whitePlayer.sendMove(moveRequestId, fromPos, toPos);

    // 9. קבלת המהלך ב-UDP בצד של Player Black ואימות הנתונים
    NetworkMessageType receivedType;
    std::vector<std::uint8_t> receivedPayload;
    bool moveReceived = blackPlayer.receiveUdpFrame(receivedType, receivedPayload, 3000);

    REQUIRE(moveReceived == true);
    REQUIRE(receivedType == NetworkMessageType::GAME_MOVE);

    auto deserializedMove = Serializer::deserializeMovePacket(receivedPayload);
    REQUIRE(deserializedMove.has_value() == true);

    CHECK(deserializedMove->matchId == whitePlayer.matchId);
    CHECK(deserializedMove->playerColor == static_cast<std::uint8_t>(PlayerColor::White));
    CHECK(deserializedMove->from.x == fromPos.x);
    CHECK(deserializedMove->from.y == fromPos.y);
    CHECK(deserializedMove->to.x == toPos.x);
    CHECK(deserializedMove->to.y == toPos.y);

    std::cout << "[Catch2 Test] Matchmaking and move relay verified successfully over UDP/TCP!" << std::endl;

    // 10. סגירה נקייה של הלופ והסרברים
    serverIo.stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::remove(dbPath.c_str()); // ניקוי בסיס הנתונים הזמני לאחר הטסט
}