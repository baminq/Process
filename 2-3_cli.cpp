#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <condition_variable>
#include <windows.h>

using namespace std;

mutex mtx;
condition_variable cv;
bool shell_sleeping = false;
bool monitor_sleeping = false;

// Function to parse command line arguments
vector<string> parseArguments(const string& command) {
    vector<string> args;
    istringstream iss(command);
    string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    return args;
}

// Function to execute a command
void executeCommand(const string& command) {
    system(command.c_str());
}

// Function to simulate executing a command with specified options
void simulateCommandExecution(const string& command, int period, int duration, int num_instances) {
    for (int i = 0; i < num_instances; ++i) {
        executeCommand(command);
        this_thread::sleep_for(chrono::seconds(period));
    }
    this_thread::sleep_for(chrono::seconds(duration));
}

// Function to handle the shell process
void shellProcess(const string& command, int period, int duration, int num_instances) {
    while (true) {
        {
            unique_lock<mutex> lock(mtx);
            shell_sleeping = false;
            cv.notify_all();
        }

        simulateCommandExecution(command, period, duration, num_instances);

        {
            unique_lock<mutex> lock(mtx);
            shell_sleeping = true;
            cv.notify_all();
        }
    }
}

// Function to handle the monitor process
void monitorProcess(const string& command, int period, int duration, int num_instances) {
    while (true) {
        {
            unique_lock<mutex> lock(mtx);
            monitor_sleeping = false;
            cv.notify_all();
        }

        if (!shell_sleeping) {
            // Execute monitor command here
            cout << "Running: [1B]" << endl;
            cout << "---------------------------" << endl;
            cout << "...prompt> ..." << endl; // Replace ... with actual output
            cout << "---------------------------" << endl;
        }

        this_thread::sleep_for(chrono::seconds(period));

        {
            unique_lock<mutex> lock(mtx);
            monitor_sleeping = true;
            cv.notify_all();
        }
    }
}

int main() {
    string command;
    int period, duration, num_instances;

    // Load configuration from commands.txt
    ifstream infile("commands.txt");
    if (infile.is_open()) {
        getline(infile, command);
        infile >> period >> duration >> num_instances;
        infile.close();
    } else {
        cerr << "Unable to open file commands.txt" << endl;
        return 1;
    }

    // Start shell and monitor threads
    thread shell_thread(shellProcess, command, period, duration, num_instances);
    thread monitor_thread(monitorProcess, command, period, duration, num_instances);

    // Wait for both threads to sleep
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, []{ return shell_sleeping && monitor_sleeping; });
    }

    // Join threads
    shell_thread.join();
    monitor_thread.join();

    return 0;
}
