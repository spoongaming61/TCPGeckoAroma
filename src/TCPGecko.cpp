#include <algorithm>
#include <arpa/inet.h>
#include <coreinit/filesystem.h>
#include <cstring>
#include <gx2/draw.h>
#include <iomanip>
#include <main.h>
#include <notifications/notification_defines.h>
#include <notifications/notifications.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <thread>
#include <vector>


// addresses vector for the find command
std::vector<uint32_t> potentialAddresses;
bool stop;
//std::vector<NotificationModuleHandle> notifHandles;

struct clientDetails {
    int32_t clientfd; // client file descriptor
    int32_t serverfd; // server file descriptor
    std::vector<int> clientList; // for storing all the client fd
    clientDetails() { // initializing the variable
        this->clientfd = -1;
        this->serverfd = -1;
    }
};

std::vector<std::string> Split(const std::string& string, const char delimiter) {
    std::string token;
    std::vector<std::string> tokens;
    std::stringstream stringStream(string);

    while (std::getline(stringStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

void stopSocket(const bool v) {
    stop = v;
}

void showNotification(const char *notification, float duration) {
    NotificationModule_SetDefaultValue(NOTIFICATION_MODULE_NOTIFICATION_TYPE_INFO,
        NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT, duration);
    NotificationModule_AddInfoNotification(notification);
}

#pragma region Peek/Poke

uint32_t Peek(const uint32_t addr) {
    if (addr >= 0x10000000) {  // Safeguard against peeking/poking an invalid address.
        // casting the address to a pointer and immediatly dereferences it
        const uint32_t val = *reinterpret_cast<uint32_t *>(static_cast<uintptr_t>(addr));
        return val;
    }
    return 0;
}

void Poke(const uint32_t addr, const uint32_t val) {
    if (addr >= 0x10000000) {
        // dereference cast of address as pointer then apply value
        *reinterpret_cast<uint32_t *>(static_cast<uintptr_t>(addr)) = val;
    }
}

/*
All of the next peek/poke functions are pretty useless but like why not
*/

uint16_t Peek16(const uint32_t addr) {
    if (addr >= 0x10000000) {
        // Peek address
        const uint32_t val = Peek(addr);
        // bitshift
        const uint16_t result = (val >> 16);
        return result;
    }
    return 0;
}

void Poke16(const uint32_t addr, const uint16_t val) {
    if (addr >= 0x10000000) {
        *reinterpret_cast<uint16_t *>(static_cast<uintptr_t>(addr)) = val;
    }
}

uint8_t Peek8(const uint32_t addr) {
    if (addr >= 0x10000000) {
        // Peek address
        const uint32_t val = Peek(addr);
        // bitshift
        const uint8_t result = (val >> 24);
        return result;
    }
    return 0;
}

void Poke8(const uint32_t addr, const uint8_t val) {
    if (addr >= 0x10000000) {
        *reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(addr)) = val;
    }
}

float PeekF32(const uint32_t addr) {
    if (addr >= 0x10000000) {
        // Peek address
        const float val = *reinterpret_cast<float *>(addr);
        return val;
    }
    return 0.0f;
}

void PokeF32(const uint32_t addr, const float val) {
    if (addr >= 0x10000000) {
        *reinterpret_cast<float *>(static_cast<uintptr_t>(addr)) = val;
    }
}

#pragma endregion PeekPoke

void Call(const uint32_t addr) {
    if (addr >= 0x10000000) {
        reinterpret_cast<void (*)()>(addr)();
    }
}

#pragma region Find Functions

void StartFindValue32(const uint32_t value) {
    potentialAddresses.clear();
    for (uint32_t i = 0x10000000; i < 0x19000000; i++) {
        if (*reinterpret_cast<uint32_t *>((uintptr_t) i) == value) {
            potentialAddresses.push_back(i);
        }
    }
}

void ContinueFindValue32(const uint32_t value) {
    std::vector<uint32_t> newSet;
    for (unsigned int potentialAddresse : potentialAddresses) {
        if (value == *reinterpret_cast<uint32_t *>(potentialAddresse)) {
            newSet.push_back(potentialAddresse);
        }
    }
    potentialAddresses = newSet;
}

#pragma endregion Find Functions

// Handles a raw request
void Commands(int client, const std::string& command) {
    std::vector<std::string> args = Split(command, ' ');
    std::string instruction = args[0];

    std::ranges::transform(instruction, instruction.begin(), ::tolower);

    if (instruction == "peek") {
        std::string address;
        std::string type;

        // gets important informations from the request
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-t") {
                type = args[i + 1];
            }
            if (args[i] == "-a") {
                address = args[i + 1];
            }
        }

        // checks for the different arguments
        if (address.empty()) {
            const char *message = "Instruction is missing address (-a)\n";
            write(client, message, strlen(message));
            return;
        }
        if (type.empty()) {
            const char *message = "Instruction is missing type (-t)\n";
            write(client, message, strlen(message));
            return;
        }
        if (strtol(address.c_str(), nullptr, 0) == 0) {
            const char *message = "Invalid address (-a)\n";
            write(client, message, strlen(message));
            return;
        }

        // this can all be simplified to a bitshift depending on the type but we have those functions ready anyway
        std::string val;
        if (type == "u8") {
            val = std::to_string(Peek8(static_cast<uint32_t>(strtol(address.c_str(),
                nullptr, 0))));
        } else if (type == "u16") {
            val = std::to_string(Peek16(static_cast<uint32_t>(strtol(address.c_str(),
                nullptr, 0))));
        } else if (type == "u32") {
            val = std::to_string(Peek(static_cast<uint32_t>(strtol(address.c_str(),
                nullptr, 0))));
        } else if (type == "f32") {
            val = std::to_string(PeekF32(static_cast<uint32_t>(strtol(address.c_str(),
                nullptr, 0))));
        } else {
            const char *message = "Invalid type (-u)\n";
            write(client, message, strlen(message));
            return;
        }

        // finally, send back our stuff to the client
        write(client, val.c_str(), strlen(val.c_str()));
        return;
    }

    if (instruction == "poke") {
        std::string address;
        std::string type;
        std::string value;

        // gets important informations from the request
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-t") {
                type = args[i + 1];
            }
            if (args[i] == "-a") {
                address = args[i + 1];
            }
            if (args[i] == "-v") {
                value = args[i + 1];
            }
        }

        // checks for the different arguments
        if (address.empty()) {
            const char *message = "Instruction is missing address (-a)\n";
            write(client, message, strlen(message));
            return;
        }
        if (type.empty()) {
            const char *message = "Instruction is missing type (-t)\n";
            write(client, message, strlen(message));
            return;
        }
        if (type.empty()) {
            const char *message = "Instruction is missing value (-v)\n";
            write(client, message, strlen(message));
            return;
        }
        if (strtol(address.c_str(), nullptr, 0) == 0) {
            const char *message = "Invalid address (-a)\n";
            write(client, message, strlen(message));
            return;
        }

        uint val = 0;
        float valf = 0;

        if (type != "f32") {
            val = std::strtoul(value.c_str(), nullptr, 0);
        } else {
            valf = std::strtof(value.c_str(), nullptr);
        }

        // this can all be simplified to a bitshift depending on the type but we have those functions ready anyway
        if (type == "u8") {
            Poke8(static_cast<uint32_t>(strtoul(address.c_str(),
                nullptr, 0)), static_cast<uint8_t>(val));
        } else if (type == "u16") {
            Poke16(static_cast<uint32_t>(strtoul(address.c_str(),
                nullptr, 0)), static_cast<uint16_t>(val));
        } else if (type == "u32") {
            Poke(static_cast<uint32_t>(strtoul(address.c_str(),
                nullptr, 0)), (uint32_t) val);
        } else if (type == "f32") {
            PokeF32(static_cast<uint32_t>(strtoul(address.c_str(),
                nullptr, 0)), valf);
        } else {
            const char *message = "Invalid type (-u)\n";
            write(client, message, strlen(message));
            return;
        }

        // writes a message to the socket so that the client knows the request has been handled
        const char *message = "ok\n";
        write(client, message, strlen(message));
        return;
    }

    if (instruction == "peekmultiple") {
        std::vector<std::string> addresses;
        std::string type;

        // gets important informations from the request
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-t") {
                type = args[i + 1];
            }
            if (args[i] == "-a") {
                addresses.emplace_back(args[i + 1]);
            }
        }

        // checks for the different arguments
        if (addresses.empty()) {
            const char *message = "Instruction is missing address (-a)\n";
            write(client, message, strlen(message));
            return;
        }
        if (type.empty()) {
            const char *message = "Instruction is missing type (-t)\n";
            write(client, message, strlen(message));
            return;
        }

        std::string values;

        // this can all be simplified to a bitshift depending on the type but we have those functions ready anyway
        for (const auto & address : addresses) {
            std::string val;
            if (type == "u8") {
                val = std::to_string(Peek8(static_cast<uint32_t>(strtol(address.c_str(),
                    nullptr, 0))));
            } else if (type == "u16") {
                val = std::to_string(Peek16(static_cast<uint32_t>(strtol(address.c_str(),
                    nullptr, 0))));
            } else if (type == "u32") {
                val = std::to_string(Peek(static_cast<uint32_t>(strtol(address.c_str(),
                    nullptr, 0))));
            } else if (type == "f32") {
                val = std::to_string(PeekF32(static_cast<uint32_t>(strtol(address.c_str(),
                    nullptr, 0))));
            } else {
                const char *message = "Invalid type (-u)\n";
                write(client, message, strlen(message));
                return;
            }
            values += val + "|";
        }
        // finally, send back our stuff to the client
        write(client, values.c_str(), strlen(values.c_str()));
        return;
    }

    if (instruction == "pokemultiple") {
        std::vector<std::string> address;
        std::string type;
        std::vector<std::string> value;

        // gets important informations from the request
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-t") {
                type = args[i + 1];
            }
            if (args[i] == "-a") {
                address.emplace_back(args[i + 1]);
            }
            if (args[i] == "-v") {
                value.emplace_back(args[i + 1]);
            }
        }

        // checks for the different arguments
        if (address.empty()) {
            const char *message = "Instruction is missing address (-a)\n";
            write(client, message, strlen(message));
            return;
        }
        if (type.empty()) {
            const char *message = "Instruction is missing type (-t)\n";
            write(client, message, strlen(message));
            return;
        }
        if (value.empty()) {
            const char *message = "Instruction is missing value (-v)\n";
            write(client, message, strlen(message));
            return;
        }

        for (size_t i = 0; i < address.size(); i++) {
            uint val = 0;
            float valf = 0;

            if (type != "f32") {
                val = std::strtoul(value[i].c_str(), nullptr, 0);
            } else {
                valf = std::strtof(value[i].c_str(), nullptr);
            }

            // this can all be simplified to a bitshift depending on the type but we have those functions ready anyway
            if (type == "u8") {
                Poke8(static_cast<uint32_t>(strtoul(address[i].c_str(),
                    nullptr, 0)), static_cast<uint8_t>(val));
            } else if (type == "u16") {
                Poke16(static_cast<uint32_t>(strtoul(address[i].c_str(),
                    nullptr, 0)), static_cast<uint16_t>(val));
            } else if (type == "u32") {
                Poke(static_cast<uint32_t>(strtoul(address[i].c_str(),
                    nullptr, 0)), (uint32_t) val);
            } else if (type == "f32") {
                PokeF32(static_cast<uint32_t>(strtoul(address[i].c_str(),
                    nullptr, 0)), valf);
            } else {
                const char *message = "Invalid type (-u)\n";
                write(client, message, strlen(message));
                return;
            }
        }
        // writes a message to the socket so that the client knows the request has been handled
        const char *message = "ok\n";
        write(client, message, strlen(message));
        return;
    }

    if (instruction == "find") {
        std::string value;
        std::string step;

        // gets important informations from the request
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-s") {
                step = args[i + 1];
            }
            if (args[i] == "-v") {
                value = args[i + 1];
            }
        }

        if (step == "first") {
            uint32_t val;
            val = std::strtol(value.c_str(), nullptr, 0);
            StartFindValue32(val);
        } else if (step == "next") {
            uint32_t val;
            val = std::strtol(value.c_str(), nullptr, 0);
            ContinueFindValue32(val);
        }
        // if we don't recognize the step we just print out the array
        else {
            for (unsigned int potentialAddress : potentialAddresses) {
                std::string addresses = std::to_string(potentialAddress) + "\n";

                const char *message = (addresses).c_str();
                write(client, message, strlen(message));
            }
            return;
        }
        // writes a message to the socket so that the client knows the request has been handled
        const char *message = "ok\n";
        write(client, message, strlen(message));
        return;
    }

    if (instruction == "pause") {
        // writes a message to the socket so that the client knows the request has been handled
        const char *message = "ok\n";
        write(client, message, strlen(message));

        // suspend the main thread
        OSThread *maint = GetMainThread();
        OSSuspendThread(maint);
        return;
    }

    if (instruction == "advance") {
        const char *message = "ok\n";
        write(client, message, strlen(message));

        OSThread *maint = GetMainThread();
        // for some reasons the OSSleepTicks work weirdly, this is very hacky and it just so happens that resuming
        // thread twice advances by a frame? idk
        OSResumeThread(maint);
        OSSleepTicks(1000);
        OSSuspendThread(maint);

        OSResumeThread(maint);
        OSSleepTicks(1000);
        OSSuspendThread(maint);

        return;
    }

    if (instruction == "resume") {
        const char *message = "ok\n";
        write(client, message, strlen(message));

        // resumes the main thread
        OSThread *maint = GetMainThread();
        OSResumeThread(maint);
        return;
    }

    if (instruction == "call") {
        std::string addr;

        // gets important informations from the request
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-a") {
                addr = args[i + 1];
            }
        }

        uint32_t val;
        val = std::strtol(addr.c_str(), nullptr, 0);
        Call(val);
        const char *message = "ok\n";
        write(client, message, strlen(message));
        return;
    }

    if (instruction == "shownotification") {
        std::string text;
        std::string arg;
        std::istringstream argsStream(command);
        float duration = 1.0f;

        while (argsStream >> std::quoted(arg))
            args.push_back(arg);

        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "-text" && i + 1 < args.size()) {
                text = args[++i];
            } else if (args[i] == "-duration") {
                duration = std::stof(args[++i]);
            }
        }

        if (!text.empty())
            showNotification(text.c_str(), duration);

        const char *message = "ok\n";
        write(client, message, strlen(message));
        return;
    }

    // TODO: Finish implementing dynamic notifications.
    // if (instruction == "createdynamicnotif") {
    //     NotificationModuleHandle dynamicNotif;
    //     std::string text = " ";
    //     std::string arg;
    //     std::istringstream argsStream(command);
    //     bool setTextColor = false;
    //     bool setBackground = false;
    //
    //     struct Color {
    //         uint8_t r = 0, g = 0, b = 0, a = 255;
    //     };
    //     Color textColor;
    //     Color bgColor;
    //
    //     while (argsStream >> std::quoted(arg))
    //         args.push_back(arg);
    //
    //     for (size_t i = 0; i < args.size(); i++) {
    //         if (args[i] == "-text" && i + 1 < args.size()) {
    //             text = args[++i];
    //         } else if (args[i] == "-textcolor" && i + 4 < args.size()) {
    //             setTextColor = true;
    //             textColor.r = std::stoi(args[++i]);
    //             textColor.g = std::stoi(args[++i]);
    //             textColor.b = std::stoi(args[++i]);
    //             textColor.a = std::stoi(args[++i]);
    //         } else if (args[i] == "-bgcolor" && i + 4 < args.size()) {
    //             setBackground = true;
    //             bgColor.r = std::stoi(args[++i]);
    //             bgColor.g = std::stoi(args[++i]);
    //             bgColor.b = std::stoi(args[++i]);
    //             bgColor.a = std::stoi(args[++i]);
    //         }
    //     }
    //
    //     NotificationModule_AddDynamicNotification(text.c_str(), &dynamicNotif);
    //     notifHandles.insert(notifHandles.begin(), dynamicNotif);
    //
    //     if (setTextColor)
    //         NotificationModule_UpdateDynamicNotificationTextColor(
    //                 dynamicNotif,
    //                 _NMColor{.r = textColor.r, .g = textColor.g, .b = textColor.b, .a = textColor.a});
    //
    //     if (setBackground)
    //         NotificationModule_UpdateDynamicNotificationBackgroundColor(
    //                 dynamicNotif, _NMColor{.r = bgColor.r, .g = bgColor.g, .b = bgColor.b, .a = bgColor.a});
    //
    //     const char *message = "ok\n";
    //     write(client, message, strlen(message));
    //     return;
    // }
    //
    // if (instruction == "updatedynamicnotif") {
    //     uint8_t index = 0;
    //     std::string text;
    //     std::string arg;
    //     std::istringstream argsStream(command);
    //     bool updateTextColor = false;
    //     bool updateBackground = false;
    //
    //     struct Color {
    //         uint8_t r = 0, g = 0, b = 0, a = 255;
    //     };
    //     Color textColor;
    //     Color bgColor;
    //
    //     while (argsStream >> std::quoted(arg))
    //         args.push_back(arg);
    //
    //     for (size_t i = 0; i < args.size(); i++) {
    //         if (args[i] == "-i" && i + 1 < args.size()) {
    //             if (index <= notifHandles.size())
    //                 index = std::stoi(args[++i]);
    //         } else if (args[i] == "-text" && i + 1 < args.size()) {
    //             text = args[++i];
    //         } else if (args[i] == "-textcolor" && i + 4 < args.size()) {
    //             updateTextColor = true;
    //             textColor.r = std::stoi(args[++i]);
    //             textColor.g = std::stoi(args[++i]);
    //             textColor.b = std::stoi(args[++i]);
    //             textColor.a = std::stoi(args[++i]);
    //         } else if (args[i] == "-bgcolor" && i + 4 < args.size()) {
    //             updateBackground = true;
    //             bgColor.r = std::stoi(args[++i]);
    //             bgColor.g = std::stoi(args[++i]);
    //             bgColor.b = std::stoi(args[++i]);
    //             bgColor.a = std::stoi(args[++i]);
    //         }
    //     }
    //
    //     if (!notifHandles.empty()) {
    //         NotificationModuleHandle dynamicNotif = notifHandles[index];
    //
    //         if (!text.empty())
    //             NotificationModule_UpdateDynamicNotificationText(dynamicNotif, text.c_str());
    //
    //         if (updateTextColor)
    //             NotificationModule_UpdateDynamicNotificationTextColor(
    //                     dynamicNotif, _NMColor{.r = textColor.r, .g = textColor.g, .b = textColor.b, .a = textColor.a});
    //
    //         if (updateBackground)
    //             NotificationModule_UpdateDynamicNotificationBackgroundColor(
    //                     dynamicNotif, _NMColor{.r = bgColor.r, .g = bgColor.g, .b = bgColor.b, .a = bgColor.a});
    //
    //     } else {
    //         const char *message = "No notifications were created\n";
    //         write(client, message, strlen(message));
    //         return;
    //     }
    //
    //     const char *message = "ok\n";
    //     write(client, message, strlen(message));
    //     return;
    // }
    //
    // if (instruction == "removedynamicnotif") {
    //     uint8_t index = 0;
    //
    //     for (size_t i = 0; i < args.size(); i++) {
    //         if (args[i] == "-i" && i + 1 < args.size())
    //             if (index <= notifHandles.size())
    //                 index = std::stoi(args[++i]);
    //     }
    //
    //     if (!notifHandles.empty()) {
    //         NotificationModuleHandle dynamicNotif = notifHandles[index];
    //
    //         NotificationModule_FinishDynamicNotification(dynamicNotif, 0);
    //         notifHandles.erase(notifHandles.begin() + index);
    //     } else {
    //         const char *message = "No notifications were created\n";
    //         write(client, message, strlen(message));
    //         return;
    //     }
    //
    //     const char *message = "ok\n";
    //     write(client, message, strlen(message));
    //     return;
    // }

    const char *message =
            "Invalid Instruction. You need to use either of the following instructions : \npeek -t "
            "(type:u8, "
            "u16, "
            "u32, f32) -a (address:0xFFFFFFFF) \npoke -t (type:u8, u16, u32, f32) -a (address:0xFFFFFFFF) "
            "-v "
            "(value:0xFF)\nfind -s (step:first, next, list) -v (value:0xFF)\npause (pauses the main wiiu "
            "thread "
            "execution)\nresume (resumes the main wiiu thread)\nadvance (advances the wiiu thread by 1 "
            "frame)\n";
    write(client, message, strlen(message));
}

int Start(int argc, const char **argv) {
    NotificationModule_InitLibrary();

    auto client = new clientDetails();

    // init server socket
    client->serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->serverfd <= 0) {
        delete client;
        return 1;
    }

    int opt = 1;
    if (setsockopt(client->serverfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof opt) < 0) {
        delete client;
        return 1;
    }

    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(7332);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(client->serverfd, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
        delete client;
        return 1;
    }
    if (listen(client->serverfd, 10) < 0) {
        delete client;
        return 1;
    }

    fd_set readfds;
    int sd = 0;

    while (!stop) {
        FD_ZERO(&readfds);
        FD_SET(client->serverfd, &readfds);
        int maxfd = client->serverfd;

        // copying the client list to readfds
        // so that we can listen to all the client
        for (const int clientsd: client->clientList) {
            FD_SET(sd, &readfds);
            if (clientsd > maxfd) {
                maxfd = clientsd;
            }
        }
        if (sd > maxfd) {
            maxfd = sd;
        }

        int activity = select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);
        if (activity < 0) {
            continue;
        }

        /*
          if something happen on client->serverfd then it means its
          new connection request
         */
        if (FD_ISSET(client->serverfd, &readfds)) {
            client->clientfd = accept(client->serverfd, (struct sockaddr *) nullptr, nullptr);
            if (client->clientfd < 0) {
                continue;
            }

            // adding client to list
            showNotification("TCPGecko: Client connected.", 3.0f);
            client->clientList.push_back(client->clientfd);
        }

        // for storing the recive message
        char message[1024];
        for (size_t i = 0; i < client->clientList.size(); ++i) {
            sd = client->clientList[i];
            if (FD_ISSET(sd, &readfds)) {
                size_t valread = read(sd, message, 1024);
                // check if client disconnected
                if (valread == 0) {
                    close(sd);
                    // remove the client from the list
                    showNotification("TCPGecko: Client disconnected.", 3.0f);
                    client->clientList.erase(client->clientList.begin() + i);
                } else {
                    std::thread t1(Commands, client->clientList[i], static_cast<std::string>(message));
                    t1.detach();
                }
                memset(message, 0, 1024);
            }
        }
    }

    close(client->serverfd);
    delete client;
    stop = false;
    return 0;
}
