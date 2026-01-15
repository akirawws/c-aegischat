#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <map>
#include <fstream>
#include <ctime>
#include "Database.h" 
#include "AuthProtocol.h" 
#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#endif

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;
Database db; 

std::map<std::string, SOCKET> onlineUsers; 
std::mutex users_mutex;

bool ReceiveExact(SOCKET s, char* buf, int size) {
    int total = 0;
    while (total < size) {
        int n = recv(s, buf + total, size - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

void BroadcastStatusToFriends(const std::string& username, uint8_t status) {
    std::vector<std::string> friends = db.GetAcceptedFriends(username);
    
    UserStatusPacket sPkt;
    sPkt.type = PACKET_USER_STATUS;
    memset(sPkt.username, 0, 64);
    strncpy(sPkt.username, username.c_str(), 63);
    sPkt.onlineStatus = status;

    std::lock_guard<std::mutex> lock(users_mutex);
    for (const auto& fName : friends) {
        if (onlineUsers.count(fName)) {
            send(onlineUsers[fName], (char*)&sPkt, sizeof(UserStatusPacket), 0);
        }
    }
}

void HandleClient(SOCKET client_socket) {
    char buffer[1024]; 
    std::string currentUsername = ""; 

    while (true) {
        ZeroMemory(buffer, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0) break;

        uint8_t packetType = (uint8_t)buffer[0];
            // === ОТЛАДКА: ЛОГ ВСЕХ ВХОДЯЩИХ ПАКЕТОВ ===
        std::cout << "\n[SERVER DEBUG] Получен пакет:" << std::endl;
        std::cout << "  Тип: " << (int)packetType << std::endl;
        std::cout << "  От пользователя (currentUsername): '" << currentUsername << "'" << std::endl;
        if (packetType == PACKET_CHAT_MESSAGE && bytesReceived >= (int)sizeof(ChatMessagePacket)) {
            ChatMessagePacket* p = (ChatMessagePacket*)buffer;
            std::cout << "  [CHAT] Отправитель: '" << p->senderUsername << "'" << std::endl;
            std::cout << "  [CHAT] Получатель: '" << p->targetUsername << "'" << std::endl;
            std::cout << "  [CHAT] Текст: '" << p->content << "'" << std::endl;
        }
        std::cout << "[SERVER DEBUG] ----------------------------------------\n" << std::endl;

        if (packetType == PACKET_LOGIN || packetType == PACKET_REGISTER) {
            AuthPacket* packet = (AuthPacket*)buffer;
            ResponsePacket res = { PACKET_AUTH_RESPONSE, false, "" };

            if (packet->type == PACKET_REGISTER) {
                if (db.RegisterUser(packet->username, packet->email, packet->password)) {
                    res.success = true;
                    strcpy(res.message, "Registered!");
                } else {
                    strcpy(res.message, "User exists!");
                }
                send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
            } 
            else if (packet->type == PACKET_LOGIN) {
                if (db.AuthenticateUser(packet->username, packet->password)) {
                    currentUsername = packet->username; 
                    res.success = true;
                    strcpy(res.message, "Welcome!");

                    {
                        std::lock_guard<std::mutex> lock(users_mutex);
                        onlineUsers[currentUsername] = client_socket;
                    }
                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);

                    BroadcastStatusToFriends(currentUsername, 1);

                    // Рассылка друзей
                    std::vector<std::string> friends = db.GetAcceptedFriends(currentUsername);
                    for (const auto& fName : friends) {
                        RoomPacket rPkt;
                        rPkt.type = PACKET_ROOM_LIST;
                        memset(rPkt.username, 0, 64);
                        strncpy(rPkt.username, fName.c_str(), 63);
                        {
                            std::lock_guard<std::mutex> lock(users_mutex);
                            rPkt.onlineStatus = (onlineUsers.count(fName) > 0) ? 1 : 0;
                        }
                        send(client_socket, (char*)&rPkt, sizeof(RoomPacket), 0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    UserProfile profile = db.GetUserProfile(currentUsername); 
                    
                    UserProfilePacket pPkt;
                    pPkt.type = PACKET_USER_PROFILE;
                    memset(pPkt.username, 0, 64);
                    memset(pPkt.display_name, 0, 64);
                    memset(pPkt.avatar_url, 0, 256);

                    strncpy(pPkt.username, currentUsername.c_str(), 63);
                    strncpy(pPkt.display_name, profile.display_name.c_str(), 63);
                    strncpy(pPkt.avatar_url, profile.avatar_url.c_str(), 255);

                    send(client_socket, (char*)&pPkt, sizeof(UserProfilePacket), 0);
                    std::cout << "[SERVER] Профиль отправлен: " << profile.display_name << std::endl;
                    // ------------------------------------

                    BroadcastStatusToFriends(currentUsername, 1);
                    
                    // --- ИНИЦИАЛИЗАЦИЯ ГРУПП ПРИ ВХОДЕ ---
                    std::vector<std::string> userGroups = db.GetUserGroups(currentUsername);
                    for (const auto& gName : userGroups) {
                        CreateGroupPacket gPkt;
                        gPkt.type = PACKET_CREATE_GROUP;
                        memset(gPkt.groupName, 0, 64);
                        strncpy(gPkt.groupName, gName.c_str(), 63);

                        std::vector<std::string> members = db.GetGroupMembers(gName);
                        gPkt.userCount = (int)members.size();
                        for (int i = 0; i < gPkt.userCount && i < 10; ++i) {
                            memset(gPkt.members[i], 0, 64);
                            strncpy(gPkt.members[i], members[i].c_str(), 63);
                        }
                        send(client_socket, (char*)&gPkt, sizeof(CreateGroupPacket), 0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    // Заявки в друзья
                    std::vector<std::string> pending = db.GetPendingRequests(currentUsername);
                    for (const auto& senderName : pending) {
                        FriendPacket fp = { PACKET_FRIEND_REQUEST };
                        strncpy(fp.senderUsername, senderName.c_str(), 63);
                        strncpy(fp.targetUsername, currentUsername.c_str(), 63);
                        send(client_socket, (char*)&fp, sizeof(FriendPacket), 0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                } else {
                    strcpy(res.message, "Invalid credentials!");
                    send(client_socket, (char*)&res, sizeof(ResponsePacket), 0);
                }
            }
        } 
        else if (packetType == PACKET_FRIEND_REQUEST) {
            FriendPacket* fPkt = (FriendPacket*)buffer;
            if (db.AddFriendRequest(fPkt->senderUsername, fPkt->targetUsername)) {
                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(fPkt->targetUsername)) {
                    send(onlineUsers[fPkt->targetUsername], (char*)fPkt, sizeof(FriendPacket), 0);
                }
            }
        }
        else if (packetType == PACKET_FRIEND_ACCEPT) {
            FriendActionPacket* aPkt = (FriendActionPacket*)buffer;
            if (db.AcceptFriendAndCreateRoom(aPkt->targetUsername, currentUsername)) {
                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(aPkt->targetUsername)) {
                    FriendActionPacket notification = { PACKET_FRIEND_ACCEPT };
                    strncpy(notification.targetUsername, currentUsername.c_str(), 63);
                    send(onlineUsers[aPkt->targetUsername], (char*)&notification, sizeof(FriendActionPacket), 0);
                }
                FriendActionPacket confirm = { PACKET_FRIEND_ACCEPT };
                strncpy(confirm.targetUsername, aPkt->targetUsername, 63);
                send(client_socket, (char*)&confirm, sizeof(FriendActionPacket), 0);
            }
        }
        else if (packetType == PACKET_CHAT_HISTORY) {
            HistoryRequestPacket* req = (HistoryRequestPacket*)buffer;
            std::vector<Message> history = db.GetChatHistory(currentUsername, req->targetUsername, req->offset, 50);

            if (history.empty()) {
                ChatHistoryEntryPacket emptyPkt = { PACKET_CHAT_HISTORY };
                emptyPkt.isLast = true;
                send(client_socket, (char*)&emptyPkt, sizeof(ChatHistoryEntryPacket), 0);
            } else {
                for (int i = (int)history.size() - 1; i >= 0; --i) {
                    ChatHistoryEntryPacket hPkt = { PACKET_CHAT_HISTORY };
                    strncpy(hPkt.senderUsername, history[i].sender.c_str(), 63);
                    strncpy(hPkt.content, history[i].text.c_str(), 511);
                    strncpy(hPkt.timestamp, history[i].timeStr.c_str(), 31);
                    hPkt.isLast = (i == 0); 
                    send(client_socket, (char*)&hPkt, sizeof(ChatHistoryEntryPacket), 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }
        else if (packetType == PACKET_DISPLAY_NAME_REPLACEMENT) {
            DisplayNameReplacementPacket* dPkt = (DisplayNameReplacementPacket*)buffer;
            
            std::cout << "[SERVER] Запрос на изменение имени:" << std::endl;
            std::cout << "  Текущий пользователь: '" << currentUsername << "'" << std::endl;
            std::cout << "  Имя из пакета: '" << dPkt->username << "'" << std::endl;
            std::cout << "  Новое имя: '" << dPkt->newDisplayName << "'" << std::endl;

            // Проверка безопасности: совпадает ли имя в пакете с текущим пользователем
            if (currentUsername.empty() || currentUsername != std::string(dPkt->username)) {
                std::cout << "[SERVER ERROR] Отказ: несовпадение имён пользователя!" << std::endl;
                continue;
            }

            // Обновляем профиль в базе данных
            if (db.UpdateUserDisplayName(currentUsername, dPkt->newDisplayName)) {
                std::cout << "[SERVER] Имя успешно обновлено в БД" << std::endl;

                // Получаем обновлённый профиль
                UserProfile profile = db.GetUserProfile(currentUsername);
                
                // Формируем пакет профиля
                UserProfilePacket pPkt;
                pPkt.type = PACKET_USER_PROFILE;
                memset(pPkt.username, 0, 64);
                memset(pPkt.display_name, 0, 64);
                memset(pPkt.avatar_url, 0, 256);

                strncpy(pPkt.username, currentUsername.c_str(), 63);
                strncpy(pPkt.display_name, profile.display_name.c_str(), 63);
                strncpy(pPkt.avatar_url, profile.avatar_url.c_str(), 255);

                // 1. Отправляем обновление самому пользователю
                send(client_socket, (char*)&pPkt, sizeof(UserProfilePacket), 0);
                std::cout << "[SERVER] Обновление профиля отправлено клиенту" << std::endl;

                // 2. Рассылаем обновление всем онлайн-друзьям
                std::vector<std::string> friends = db.GetAcceptedFriends(currentUsername);
                std::lock_guard<std::mutex> lock(users_mutex);
                
                for (const auto& friendName : friends) {
                    if (onlineUsers.count(friendName)) {
                        send(onlineUsers[friendName], (char*)&pPkt, sizeof(UserProfilePacket), 0);
                        std::cout << "[SERVER] Обновление профиля отправлено другу: " << friendName << std::endl;
                    }
                }
            } else {
                std::cout << "[SERVER ERROR] Не удалось обновить имя в базе данных!" << std::endl;
            }
        }
            else if (packetType == PACKET_BIO_REPLACEMENT) {
                // Накладываем структуру на уже считанный buffer
                BioReplacementPacket* pkt = (BioReplacementPacket*)buffer;
                
                // Безопасно копируем в строку
                char safeBio[256];
                memset(safeBio, 0, 256);
                memcpy(safeBio, pkt->bio, 255);
                
                std::string newBio = safeBio;

                if (!currentUsername.empty()) {
                    std::cout << "[SERVER] Смена BIO для " << currentUsername << ": " << newBio << std::endl;
                    if (db.UpdateUserBio(currentUsername, newBio)) {
                        std::cout << "[SERVER] BIO обновлено в базе." << std::endl;
                        
                        // Отправляем профиль назад (как у вас и было)
                        UserProfile profile = db.GetUserProfile(currentUsername);
                        UserProfilePacket resp;
                        memset(&resp, 0, sizeof(resp));
                        resp.type = PACKET_USER_PROFILE;
                        strncpy(resp.username, profile.username.c_str(), 63);
                        strncpy(resp.display_name, profile.display_name.c_str(), 63);
                        strncpy(resp.avatar_url, profile.avatar_url.c_str(), 255);
                        send(client_socket, (char*)&resp, sizeof(resp), 0);
                    }
                }
            }


        else if (packetType == PACKET_GET_AVATAR) {
            GetAvatarPacket* req = (GetAvatarPacket*)buffer;
            std::string targetUser = req->username;

            // 1. Получаем профиль из БД, чтобы узнать путь к файлу
            UserProfile profile = db.GetUserProfile(targetUser);
            std::string path = profile.avatar_url;

            // 2. Проверяем существование файла
            if (path.empty() || !fs::exists(path)) {
                AvatarHeader header;
                header.type = PACKET_AVATAR_DATA;
                header.fileSize = 0; // Сообщаем клиенту, что аватара нет
                send(client_socket, (char*)&header, sizeof(header), 0);
            } else {
                // 3. Открываем файл и определяем размер
                std::ifstream file(path, std::ios::binary | std::ios::ate);
                if (file.is_open()) {
                    std::streamsize size = file.tellg();
                    file.seekg(0, std::ios::beg);

                    // 4. Формируем и отправляем заголовок
                    AvatarHeader header;
                    header.type = PACKET_AVATAR_DATA;
                    header.fileSize = (uint32_t)size;
                    std::string ext = fs::path(path).extension().string();
                    memset(header.extension, 0, 8);
                    strncpy(header.extension, ext.c_str(), 7);

                    send(client_socket, (char*)&header, sizeof(header), 0);

                    // 5. Читаем файл и отправляем данные
                    std::vector<char> fileBuffer(size);
                    if (file.read(fileBuffer.data(), size)) {
                        send(client_socket, fileBuffer.data(), (int)size, 0);
                        std::cout << "[SERVER] Отправлен аватар: " << targetUser << " (" << size << " байт)" << std::endl;
                    }
                }
            }
        }


        else if (packetType == PACKET_AVATAR_UPDATE) {
            // 1. Извлекаем заголовок из того, что уже прочитали в buffer
            AvatarHeader header;
            if (bytesReceived < sizeof(AvatarHeader)) {
                // Если заголовок не влез в первый recv, дочитываем его
                memcpy(&header, buffer, bytesReceived);
                ReceiveExact(client_socket, ((char*)&header) + bytesReceived, sizeof(AvatarHeader) - bytesReceived);
            } else {
                memcpy(&header, buffer, sizeof(AvatarHeader));
            }

            uint32_t fileSize = header.fileSize;
            std::cout << "[SERVER] Ожидаемый размер файла: " << fileSize << " байт" << std::endl;

            if (fileSize > 10 * 1024 * 1024) { // Защита от слишком больших файлов
                std::cout << "[SERVER ERROR] Файл слишком большой!" << std::endl;
                continue;
            }

            std::vector<char> imageData(fileSize);

            // 2. ВАЖНО: Проверяем, сколько байт самой картинки УЖЕ находятся в buffer
            // (они прилетели вместе с заголовком в одном recv)
            int headerSize = sizeof(AvatarHeader);
            int alreadyReadPayload = bytesReceived - headerSize;

            if (alreadyReadPayload > 0) {
                if (alreadyReadPayload > (int)fileSize) alreadyReadPayload = fileSize;
                memcpy(imageData.data(), buffer + headerSize, alreadyReadPayload);
            }

            // 3. Дочитываем строго оставшуюся часть
            int remaining = fileSize - alreadyReadPayload;
            if (remaining > 0) {
                if (!ReceiveExact(client_socket, imageData.data() + alreadyReadPayload, remaining)) {
                    std::cout << "[SERVER ERROR] Ошибка при дочитывании данных" << std::endl;
                    break;
                }
            }

            // 4. Сохранение (ваш существующий код)
            std::string fileName = currentUsername + "_" + std::to_string(time(0)) + header.extension;
            std::string relativePath = "assets/avatar_url/" + fileName;
            
            if (!fs::exists("assets/avatar_url/")) fs::create_directories("assets/avatar_url/");

            std::ofstream outFile(relativePath, std::ios::binary);
            if (outFile.is_open()) {
                outFile.write(imageData.data(), fileSize);
                outFile.close();
                std::cout << "[SERVER] !!! ФАЙЛ УСПЕШНО ЗАПИСАН !!!" << std::endl;
                db.UpdateUserAvatar(currentUsername, relativePath);
            }
        }
        else if (packetType == PACKET_CREATE_GROUP) {
            CreateGroupPacket* gPkt = (CreateGroupPacket*)buffer;
            std::vector<std::string> memberList;
            memberList.push_back(currentUsername);
            for (int i = 0; i < gPkt->userCount; i++) {
                memberList.push_back(gPkt->members[i]);
            }

            if (db.CreateGroup(gPkt->groupName, memberList)) {
                std::lock_guard<std::mutex> lock(users_mutex);
                for (const auto& member : memberList) {
                    if (onlineUsers.count(member)) {
                        send(onlineUsers[member], (char*)gPkt, sizeof(CreateGroupPacket), 0);
                    }
                }
            }
        }
    else if (packetType == PACKET_CHAT_MESSAGE) {
        std::cout << "\n[SERVER] ===== ПОЛУЧЕН PACKET_CHAT_MESSAGE =====" << std::endl;
        
        // Данные УЖЕ находятся в buffer, так как мы сделали recv в начале цикла.
        // Накладываем структуру на буфер:
        ChatMessagePacket* p = (ChatMessagePacket*)buffer;

        char sender[64] = {0};
        char target[64] = {0};
        char content[512] = {0};
        
        // Копируем данные из структуры
        strncpy(sender, p->senderUsername, 63);
        strncpy(target, p->targetUsername, 63);
        strncpy(content, p->content, 511);
        
        std::cout << "[SERVER] Пакет извлечен из буфера:" << std::endl;
        std::cout << "[SERVER] Отправитель: '" << sender << "'" << std::endl;
        std::cout << "[SERVER] Получатель: '" << target << "'" << std::endl;
        std::cout << "[SERVER] Содержимое: '" << content << "'" << std::endl;
    
    if (currentUsername == std::string(sender)) {
        std::cout << "[SERVER] Проверка отправителя: OK (текущий пользователь совпадает с отправителем)" << std::endl;
        
        // Проверяем, группа ли это
        bool isGroup = db.IsGroup(target);
        std::cout << "[SERVER] Это группа? " << (isGroup ? "ДА" : "НЕТ") << std::endl;
        
        if (db.SaveMessage(sender, target, content)) {
            std::cout << "[SERVER] Сообщение сохранено в БД" << std::endl;
            
            if (isGroup) {
                std::vector<std::string> members = db.GetGroupMembers(target);
                std::cout << "[SERVER] Члены группы (" << members.size() << "): ";
                for (const auto& m : members) std::cout << m << " ";
                std::cout << std::endl;
                
                // Создаем полный пакет для отправки другим участникам
                ChatMessagePacket fullPkt;
                fullPkt.type = PACKET_CHAT_MESSAGE;
                strncpy(fullPkt.senderUsername, sender, 63);
                strncpy(fullPkt.targetUsername, target, 63);
                strncpy(fullPkt.content, content, 511);
                
                std::lock_guard<std::mutex> lock(users_mutex);
                for (const auto& m : members) {
                    if (m != currentUsername && onlineUsers.count(m)) {
                        send(onlineUsers[m], (char*)&fullPkt, sizeof(ChatMessagePacket), 0);
                        std::cout << "[SERVER] Отправлено участнику: " << m << std::endl;
                    }
                }
            } else {
                // Создаем полный пакет для отправки получателю
                ChatMessagePacket fullPkt;
                fullPkt.type = PACKET_CHAT_MESSAGE;
                strncpy(fullPkt.senderUsername, sender, 63);
                strncpy(fullPkt.targetUsername, target, 63);
                strncpy(fullPkt.content, content, 511);
                
                std::lock_guard<std::mutex> lock(users_mutex);
                if (onlineUsers.count(target)) {
                    send(onlineUsers[target], (char*)&fullPkt, sizeof(ChatMessagePacket), 0);
                    std::cout << "[SERVER] Отправлено пользователю: " << target << std::endl;
                }
            }
        } else {
            std::cout << "[SERVER ERROR] Не удалось сохранить сообщение в БД!" << std::endl;
        }
    } else {
        std::cout << "[SERVER WARNING] Отправитель не совпадает с текущим пользователем!" << std::endl;
        std::cout << "[SERVER WARNING] currentUsername: '" << currentUsername << "', sender: '" << sender << "'" << std::endl;
    }
    
    std::cout << "[SERVER] ===== ОБРАБОТКА PACKET_CHAT_MESSAGE ЗАВЕРШЕНА =====\n" << std::endl;
}
}

    if (!currentUsername.empty()) {
        BroadcastStatusToFriends(currentUsername, 0);
        std::lock_guard<std::mutex> lock(users_mutex);
        onlineUsers.erase(currentUsername);
    }
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
    }
    closesocket(client_socket);
}

int main() {
    SetConsoleCP(65001); 
    SetConsoleOutputCP(65001);
    
    if (!db.Connect()) return 1;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(5555);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&hint, sizeof(hint));
    listen(serverSocket, SOMAXCONN);

    std::cout << "[SERVER] AEGIS Online на порту 5555" << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket != INVALID_SOCKET) {
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.push_back(clientSocket);
            }
            std::thread(HandleClient, clientSocket).detach();
        }
    }

    db.Disconnect();
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}