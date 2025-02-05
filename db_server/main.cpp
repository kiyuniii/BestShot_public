#include <iostream>
#include <string>
#include <cstring>
#include <sqlite3.h>    // sudo apt install libsqlite3-dev 
#include <csignal>
#include <sys/stat.h>
#include <dirent.h>     // Directroy Entry
#include <pugixml.hpp>  // sudo apt install libpugixml-dev
#include <pugixml.hpp>
#include <thread>
#include <chrono>
#include <regex>

sqlite3* db;

std::string find_latest_file(const std::string& dir_path) {
    DIR* dir = opendir(dir_path.c_str());
    if(!dir) {
        std::cerr << "Cannot open directory: " << dir_path << std::endl;
        return "";
    }

    std::string latest_file;
    time_t latest_mod_time = 0;

    struct dirent* entry;
    while((entry = readdir(dir)) != nullptr) {
        if(strncmp(entry->d_name, "metadata", 8) == 0 &&
            strstr(entry->d_name, ".txt") != nullptr) {
            std::string file_path = dir_path + "/" + entry->d_name;

            /* Update latest file by comparing modification time */
            struct stat file_stat;
            if(stat(file_path.c_str(), &file_stat) == 0) {
                if(file_stat.st_mtime > latest_mod_time) {
                    latest_mod_time = file_stat.st_mtime;
                    latest_file = file_path;
                }
            }
        }
    }
    closedir(dir);

    return latest_file;
}

bool createTables(sqlite3* db) {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS Object (
            ObjectId INTEGER PRIMARY KEY,
            Type TEXT,
            Likelihood REAL,
            plateNumber TEXT
        );

        CREATE TABLE IF NOT EXISTS ImageInfo (
            ObjectId INTEGER PRIMARY KEY,
            UtcTime TEXT,
            ImageRef TEXT,
            FOREIGN KEY(ObjectId) REFERENCES Object(ObjectId)
        );
    )";

    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}


/* Insert into Object table */
bool insertObject(sqlite3* db, int objectId, const std::string& Type, double Likelihood, const std::string& plateNumber) {
    const char* sql = R"(
        INSERT INTO Object (ObjectId, Type, Likelihood, plateNumber)
        VALUES (?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, objectId);
    sqlite3_bind_text(stmt, 2, Type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, Likelihood);
    sqlite3_bind_text(stmt, 4, plateNumber.c_str(), -1, SQLITE_STATIC);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) 
        std::cerr << "Failed to insert object data: " << sqlite3_errmsg(db) << std::endl;
    
    sqlite3_finalize(stmt);
    return success;
}

/* Insert into ImageInfo table */
bool insertImageInfo(sqlite3* db, int objectId, const std::string& utcTime, const std::string& imageRef) {
    const char* sql = R"(
        INSERT INTO ImageInfo (ObjectId, UtcTime, ImageRef)
        VALUES (?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, objectId);
    sqlite3_bind_text(stmt, 2, utcTime.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, imageRef.c_str(), -1, SQLITE_STATIC);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success) 
        std::cerr << "Failed to insert image info: " << sqlite3_errmsg(db) << std::endl;
    
    sqlite3_finalize(stmt);
    return success;
}

void parseAndStoreData(const std::string& xmlData, sqlite3* db) {
    pugi::xml_document doc;
    if (!doc.load_file(xmlData.c_str())) {
        std::cerr << "Failed to load XML file." << std::endl;
        return;
    }

    // Start transaction
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    try {
        pugi::xml_node frameNode = doc.child("tt:MetadataStream").child("tt:VideoAnalytics").child("tt:Frame");
        std::string utcTime = frameNode.attribute("UtcTime").as_string();

        for (pugi::xml_node objectNode : frameNode.children("tt:Object")) {
            // Look for ImageRef first
            pugi::xml_node imageRefNode = objectNode.child("tt:Appearance").child("tt:ImageRef");
            if (!imageRefNode) {
                continue; // Skip objects without ImageRef
            }

            int objectId = objectNode.attribute("ObjectId").as_int();
            std::string imageRef = imageRefNode.text().as_string();
            
            // Get Type and Likelihood from Class
            pugi::xml_node classNode = objectNode.child("tt:Appearance").child("tt:Class");
            std::string type = classNode.child("tt:Type").text().as_string();
            double likelihood = classNode.child("tt:Type").attribute("Likelihood").as_double();

            // Get plate number if exists
            std::string plateNumber;
            pugi::xml_node licensePlateInfoNode = objectNode.child("tt:Appearance").child("tt:LicensePlateInfo");
            if (licensePlateInfoNode) {
                pugi::xml_node plateNumberNode = licensePlateInfoNode.child("tt:PlateNumber");
                if (plateNumberNode) {
                    plateNumber = plateNumberNode.text().as_string();
                }
            }

            // Only insert objects with ImageRef
            if (!insertObject(db, objectId, type, likelihood, plateNumber)) {
                throw std::runtime_error("Failed to insert object data");
            }

            if (!insertImageInfo(db, objectId, utcTime, imageRef)) {
                throw std::runtime_error("Failed to insert image info");
            }
        }
        // Commit transaction
        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to commit transaction");
        }

    } catch (const std::exception& e) {
        std::cerr << "Error during parsing: " << e.what() << std::endl;
        // Rollback transaction on error
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}


void monitorDirectory(const std::string& dir_path, sqlite3* db) {
    std::string last_file;
    
    while (true) {
        std::string latest_file = find_latest_file(dir_path);

        if (!latest_file.empty() && (latest_file != last_file)) {
            std::cout << "Latest Metadata file: " << latest_file << std::endl;
            parseAndStoreData(latest_file, db);
            last_file = latest_file;
        } else {
            std::cout << "Waiting for new metadata file..." << std::endl; 
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));  // 1珥덈쭏�� �붾젆�좊━ �뺤씤
    }
}

void sigintHandler(int signum) {
    std::cout << "\nProgram exiting safely..." << std::endl;
    if (db) {
        sqlite3_close(db);
        std::cout << "Database closed." << std::endl;
    }
    exit(signum);
}

int main() {
    signal(SIGINT, sigintHandler);

    /* �� �ㅽ겕由쏀듃濡� �먮룞�뷀빐�쇳븿(Linux/Windows/Mac)
     * - ex) int main(int argc, char** argv) */
    std::string dir_path = "/home/kiyun/Downloads";
    std::string latest_file = find_latest_file(dir_path);

   sqlite3* db;
    if (sqlite3_open("metadata.db", &db) != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    if (!createTables(db)) {
        std::cerr << "Failed to create tables." << std::endl;
        sqlite3_close(db);
        return 1;
    }

    monitorDirectory(dir_path, db);

    sqlite3_close(db);
    return 0;
}
