#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <iomanip>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <lmcons.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#=include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#endif

namespace fs = std::filesystem;

class FileExplorer {
private:
    fs::path currentPath;
    std::vector<fs::directory_entry> currentEntries;
    
    void listDirectory() {
        currentEntries.clear();
        for (const auto& entry : fs::directory_iterator(currentPath)) {
            currentEntries.push_back(entry);
        }
        
        // Sort entries: directories first, then files
        std::sort(currentEntries.begin(), currentEntries.end(), 
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
                if (is_directory(a) != is_directory(b))
                    return is_directory(a);
                return a.path().filename().string() < b.path().filename().string();
            });
    }
    
    void printFileInfo(const fs::directory_entry& entry) {
        // File type and permissions
#ifdef _WIN32
        std::wstring wpath = entry.path().wstring();
        DWORD attr = GetFileAttributesW(wpath.c_str());
        bool isDir = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
        std::cout << (isDir ? "d" : "-") << "rwxrwxrwx";
        
        // Owner and group (simplified for Windows)
        std::string owner = "user";
        std::string group = "group";
        
        // File size
        uintmax_t fileSize = 0;
        try {
            if (!isDir) {
                fileSize = entry.file_size();
            }
        } catch (const std::filesystem::filesystem_error&) {
            // In case of permission issues
            fileSize = 0;
        }
        
        std::cout << " " << std::left << std::setw(8) << owner;
        std::cout << " " << std::left << std::setw(8) << group;
        std::cout << " " << std::right << std::setw(8) << fileSize;
#else
        struct stat fileStat;
        stat(entry.path().c_str(), &fileStat);
        
        // Print permissions
        std::cout << ((S_ISDIR(fileStat.st_mode)) ? "d" : "-");
        std::cout << ((fileStat.st_mode & S_IRUSR) ? "r" : "-");
        std::cout << ((fileStat.st_mode & S_IWUSR) ? "w" : "-");
        std::cout << ((fileStat.st_mode & S_IXUSR) ? "x" : "-");
        std::cout << ((fileStat.st_mode & S_IRGRP) ? "r" : "-");
        std::cout << ((fileStat.st_mode & S_IWGRP) ? "w" : "-");
        std::cout << ((fileStat.st_mode & S_IXGRP) ? "x" : "-");
        std::cout << ((fileStat.st_mode & S_IROTH) ? "r" : "-");
        std::cout << ((fileStat.st_mode & S_IWOTH) ? "w" : "-");
        std::cout << ((fileStat.st_mode & S_IXOTH) ? "x" : " ");
        
        // Print owner and group
        struct passwd *pw = getpwuid(fileStat.st_uid);
        struct group *gr = getgrgid(fileStat.st_gid);
        std::cout << " " << std::left << std::setw(8) << (pw ? pw->pw_name : "");
        std::cout << " " << std::left << std::setw(8) << (gr ? gr->gr_name : "");
        
        // Print size
        std::cout << " " << std::right << std::setw(8) << fileStat.st_size;
#endif
        
        // Print name with color
        std::string color = is_directory(entry) ? "\033[1;34m" : "\033[0m";
        std::cout << " " << color << entry.path().filename().string() << "\033[0m" << std::endl;
    }
    
public:
    FileExplorer() {
        currentPath = fs::current_path();
        listDirectory();
    }
    
    void list() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
        std::cout << "Current directory: " << currentPath << "\n\n";
        
        for (const auto& entry : currentEntries) {
            printFileInfo(entry);
        }
        
        std::cout << "\nCommands: cd <dir>, mkdir <name>, rm <name>, cp <src> <dest>, mv <src> <dest>, search <name>, exit\n";
    }
    
    void changeDirectory(const std::string& path) {
        fs::path newPath = currentPath / path;
        if (fs::exists(newPath) && fs::is_directory(newPath)) {
            currentPath = fs::canonical(newPath);
            listDirectory();
        } else {
            std::cerr << "Directory not found: " << path << std::endl;
        }
    }
    
    void createDirectory(const std::string& name) {
        try {
            fs::create_directory(currentPath / name);
            listDirectory();
        } catch (const std::exception& e) {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
        }
    }
    
    void removeFile(const std::string& name) {
        try {
            fs::path target = currentPath / name;
            if (fs::is_directory(target)) {
                fs::remove_all(target);
            } else {
                fs::remove(target);
            }
            listDirectory();
        } catch (const std::exception& e) {
            std::cerr << "Error removing: " << e.what() << std::endl;
        }
    }
    
    void copyFile(const std::string& src, const std::string& dest) {
        try {
            fs::path sourcePath = (src[0] == '/') ? src : currentPath / src;
            fs::path destPath = (dest[0] == '/') ? dest : currentPath / dest;
            
            if (fs::is_directory(sourcePath)) {
                fs::create_directories(destPath);
                fs::copy(sourcePath, destPath, fs::copy_options::recursive);
            } else {
                fs::copy(sourcePath, destPath, fs::copy_options::overwrite_existing);
            }
            listDirectory();
        } catch (const std::exception& e) {
            std::cerr << "Error copying: " << e.what() << std::endl;
        }
    }
    
    void moveFile(const std::string& src, const std::string& dest) {
        try {
            fs::path sourcePath = (src[0] == '/') ? src : currentPath / src;
            fs::path destPath = (dest[0] == '/') ? dest : currentPath / dest;
            
            fs::rename(sourcePath, destPath);
            listDirectory();
        } catch (const std::exception& e) {
            std::cerr << "Error moving: " << e.what() << std::endl;
        }
    }
    
    void search(const std::string& pattern) {
        std::cout << "Searching for: " << pattern << " in " << currentPath << "\n";
        size_t count = 0;
        
        for (const auto& entry : fs::recursive_directory_iterator(currentPath)) {
            std::string filename = entry.path().filename().string();
            if (filename.find(pattern) != std::string::npos) {
                std::cout << entry.path().string() << std::endl;
                count++;
            }
        }
        
        std::cout << "\nFound " << count << " matches.\n";
    }
};

int main() {
    FileExplorer explorer;
    std::string command;
    
    while (true) {
        explorer.list();
        
        std::cout << "\n> ";
        std::getline(std::cin, command);
        
        if (command == "exit") {
            break;
        } else if (command.substr(0, 3) == "cd ") {
            explorer.changeDirectory(command.substr(3));
        } else if (command.substr(0, 6) == "mkdir ") {
            explorer.createDirectory(command.substr(6));
        } else if (command.substr(0, 3) == "rm ") {
            explorer.removeFile(command.substr(3));
        } else if (command.substr(0, 3) == "cp ") {
            size_t space = command.find(' ', 3);
            if (space != std::string::npos) {
                std::string src = command.substr(3, space - 3);
                std::string dest = command.substr(space + 1);
                explorer.copyFile(src, dest);
            }
        } else if (command.substr(0, 3) == "mv ") {
            size_t space = command.find(' ', 3);
            if (space != std::string::npos) {
                std::string src = command.substr(3, space - 3);
                std::string dest = command.substr(space + 1);
                explorer.moveFile(src, dest);
            }
        } else if (command.substr(0, 7) == "search ") {
            explorer.search(command.substr(7));
            std::cout << "Press Enter to continue...";
            std::cin.ignore();
        }
    }
    
    return 0;
}
