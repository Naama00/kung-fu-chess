#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include "NetworkServer.hpp"
#include "MatchManager.hpp"

int main() {
    try {
        const std::uint16_t port = 8080;
        boost::asio::io_context ioContext;

        // העברת ה-ioContext כדי שה-MatchManager יוכל להגדיר טיימרים לכל משחק
        kungfu::MatchManager matchManager(ioContext);

        std::cout << "Starting KungFu Chess Server on port " << port << "..." << std::endl;
        kungfu::NetworkServer server(ioContext, port, matchManager);

        std::cout << "Server is running. Waiting for players to connect..." << std::endl;

        // כרגע ה-io_context רץ מ-thread יחיד. כל NetworkSession וכל LiveMatch
        // כבר מוגנים ע"י ה-strand הפרטי שלהם, ו-MatchManager מוגן ע"י mutex -
        // כך שכאשר יהיה צורך לתמוך בכמות גדולה יותר של משחקים במקביל, אפשר
        // לעבור להרצה מרובת-threads פשוט ע"י פתיחת כמה threads שכל אחד
        // קורא ioContext.run(), ללא צורך בשינוי מבני בקוד:
        //
        //   std::vector<std::thread> pool;
        //   for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
        //       pool.emplace_back([&ioContext]() { ioContext.run(); });
        //   }
        //   for (auto& t : pool) t.join();
        ioContext.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal Server Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}