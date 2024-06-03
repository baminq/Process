#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <windows.h>

using namespace std;

mutex mtx;
condition_variable cv;
bool shell_sleeping = false;
bool monitor_sleeping = false;

void print_output(string running_pid, vector<string>& dq, vector<map<string, int>>& wq) {
    lock_guard<mutex> lock(mtx);
    cout << "Running: [" << running_pid << "]" << endl;
    cout << "---------------------------" << endl;

    if (dq.empty()) {
        cout << "DQ: P => [0F] (bottom/top)" << endl;
    } else {
        cout << "DQ:      [";
        for (const auto& pid : dq) {
            cout << pid << " ";
        }
        cout << "] (bottom)" << endl;
        
        cout << "   P => [";
        bool promoted_printed = false;
        for (const auto& item : wq) {
            for (const auto& entry : item) {
                if (entry.second == 0 && !promoted_printed) {
                    cout << "*" << running_pid << " ";
                    promoted_printed = true;
                } else {
                    cout << entry.first << " ";
                }
            }
        }
        cout << "]" << endl;

        cout << "            [";
        for (const auto& item : wq) {
            for (const auto& entry : item) {
                cout << entry.first << " ";
            }
        }
        cout << "0F] (top)" << endl;
    }
    cout << "---------------------------" << endl;
    cout << "WQ:      [";
    for (const auto& item : wq) {
        for (const auto& entry : item) {
            cout << entry.first << ":" << entry.second << " ";
        }
    }
    cout << "]" << endl;
}

void monitor_process(string& running_pid, vector<string>& dq, vector<map<string, int>>& wq, int x) {
    while (true) {
        {
            unique_lock<mutex> lock(mtx);
            monitor_sleeping = false;
            cv.notify_all();
        }
        print_output(running_pid, dq, wq);
        Sleep(x * 1000);  // Sleep for x seconds
        {
            unique_lock<mutex> lock(mtx);
            monitor_sleeping = true;
            cv.notify_all();
        }
    }
}

char** parse(const char* command) {
    vector<char*> tokens;
    char* token = strtok(const_cast<char*>(command), " ");
    while (token != nullptr) {
        tokens.push_back(token);
        token = strtok(nullptr, " ");
    }
    tokens.push_back(nullptr);

    char** args = new char*[tokens.size()];
    for (size_t i = 0; i < tokens.size(); ++i) {
        args[i] = tokens[i];
    }
    return args;
}

void exec(char** args) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    string command;
    for (size_t i = 0; args[i] != nullptr; ++i) {
        command += args[i];
        command += " ";
    }

    // Add null terminator to the command string
    command += '\0';

    // Create a writable copy of the command string
    char* writable_command = new char[command.size()];
    strcpy(writable_command, command.c_str());

    if (!CreateProcess(
            nullptr, 
            writable_command, 
            nullptr, 
            nullptr, 
            FALSE, 
            0, 
            nullptr, 
            nullptr, 
            &si, 
            &pi)) {
        cerr << "CreateProcess failed: " << GetLastError() << endl;
    } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    delete[] args;
    delete[] writable_command;
}

void shell_process(string& running_pid, vector<string>& dq, vector<map<string, int>>& wq, int y) {
    int pid_counter = 2;
    while (true) {
        {
            unique_lock<mutex> lock(mtx);
            shell_sleeping = false;
            cv.notify_all();
        }

        string command;
        getline(cin, command);
        char** args = parse(command.c_str());
        exec(args);

        dq.push_back(to_string(pid_counter++) + "B");
        Sleep(y * 1000);  // Sleep for y seconds

        {
            unique_lock<mutex> lock(mtx);
            shell_sleeping = true;
            cv.notify_all();
        }
    }
}

int main() {
    string running_pid = "1B";
    vector<string> dq = {"2B", "7B", "4B"};
    vector<map<string, int>> wq = { {{"15F", 2}}, {{"10B", 5}} };

    int x = 5; // Monitor print interval
    int y = 5; // Shell sleep interval

    thread monitor_thread(monitor_process, ref(running_pid), ref(dq), ref(wq), x);
    thread shell_thread(shell_process, ref(running_pid), ref(dq), ref(wq), y);

    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, []{ return shell_sleeping && monitor_sleeping; });
    }

    monitor_thread.join();
    shell_thread.join();

    return 0;
}
