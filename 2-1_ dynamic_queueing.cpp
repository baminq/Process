#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <windows.h> 

using namespace std;

mutex mtx;
condition_variable cv;
bool stop = false;

struct ProcessNode {
    int process_id;
    ProcessNode* next;

    ProcessNode(int id) : process_id(id), next(nullptr) {}
};

struct StackNode {
    ProcessNode* process_list;
    StackNode* next;

    StackNode() : process_list(nullptr), next(nullptr) {}
};

class DynamicQueue {
private:
    StackNode* top;
    StackNode* bottom;
    StackNode* promote_pointer;
    int total_processes;
    int threshold;

    void calculate_threshold() {
        int stack_count = 0;
        StackNode* current = top;
        while (current != nullptr) {
            stack_count++;
            current = current->next;
        }
        if (stack_count > 0) {
            threshold = total_processes / stack_count;
        } else {
            threshold = 1;
        }
    }

public:
    DynamicQueue() : top(nullptr), bottom(nullptr), promote_pointer(nullptr), total_processes(0), threshold(1) {}

    void enqueue(int process_id, bool is_foreground) {
        ProcessNode* new_process = new ProcessNode(process_id);
        
        if (is_foreground) {
            if (top == nullptr) {
                top = new StackNode();
                bottom = top;
                top->process_list = new_process;
                promote_pointer = top;
            } else {
                ProcessNode* temp = top->process_list;
                if (temp == nullptr) {
                    top->process_list = new_process;
                } else {
                    while (temp->next != nullptr) {
                        temp = temp->next;
                    }
                    temp->next = new_process;
                }
            }
        } else { // is_background
            StackNode* bg_node = new StackNode();
            bg_node->process_list = new_process;
            
            if (top == nullptr) {
                top = bg_node;
                bottom = top;
                promote_pointer = top;
            } else {
                bottom->next = bg_node;
                bottom = bg_node;
            }
        }

        total_processes++;
        calculate_threshold();
        promote();
        split_n_merge();
    }

    ProcessNode* dequeue() {
        if (top == nullptr || top->process_list == nullptr) {
            return nullptr;
        }

        ProcessNode* process_to_dispatch = top->process_list;
        top->process_list = top->process_list->next;

        if (top->process_list == nullptr) {
            StackNode* temp = top;
            top = top->next;
            delete temp;

            if (promote_pointer == temp) {
                promote_pointer = top;
            }
        }

        total_processes--;
        calculate_threshold();
        return process_to_dispatch;
    }

    void promote() {
        if (promote_pointer == nullptr) return;

        StackNode* current = promote_pointer;
        promote_pointer = promote_pointer->next;

        if (promote_pointer == nullptr) {
            promote_pointer = top;
        }

        if (current->next == nullptr) return;

        ProcessNode* moving_node = current->process_list;
        current->process_list = current->process_list->next;

        if (current->process_list == nullptr) {
            remove_stack_node(current);
        }

        StackNode* target = get_previous_stack_node(current);
        if (target == nullptr) {
            top = new StackNode();
            top->process_list = moving_node;
            top->next = current;
        } else {
            ProcessNode* temp = target->process_list;
            if (temp == nullptr) {
                target->process_list = moving_node;
            } else {
                while (temp->next != nullptr) {
                    temp = temp->next;
                }
                temp->next = moving_node;
            }
        }
    }

    void split_n_merge() {
        StackNode* current = top;
        while (current != nullptr) {
            int length = get_process_list_length(current->process_list);
            if (length > threshold) {
                ProcessNode* split_head = current->process_list;
                ProcessNode* split_tail = split_list(current->process_list, length / 2);
                current->process_list = split_tail;

                StackNode* new_node = new StackNode();
                new_node->process_list = split_head;
                new_node->next = current->next;
                current->next = new_node;

                if (current == bottom) {
                    bottom = new_node;
                }
            }
            current = current->next;
        }
    }

    void remove_stack_node(StackNode* node) {
        if (top == node) {
            top = top->next;
            if (promote_pointer == node) {
                promote_pointer = top;
            }
            delete node;
        } else {
            StackNode* current = top;
            while (current->next != node) {
                current = current->next;
            }
            current->next = node->next;
            if (bottom == node) {
                bottom = current;
            }
            if (promote_pointer == node) {
                promote_pointer = current->next;
            }
            delete node;
        }
    }

    StackNode* get_previous_stack_node(StackNode* node) {
        if (node == top) {
            return nullptr;
        }

        StackNode* current = top;
        while (current->next != node) {
            current = current->next;
        }
        return current;
    }

    ProcessNode* split_list(ProcessNode* head, int position) {
        if (head == nullptr) return nullptr;
        ProcessNode* current = head;
        for (int i = 1; i < position && current != nullptr; ++i) {
            current = current->next;
        }
        if (current == nullptr) return nullptr;
        ProcessNode* split_head = current->next;
        current->next = nullptr;
        return split_head;
    }

    int get_process_list_length(ProcessNode* head) {
        int length = 0;
        while (head != nullptr) {
            ++length;
            head = head->next;
        }
        return length;
    }

    void display() {
        StackNode* current = top;
        while (current != nullptr) {
            ProcessNode* process_current = current->process_list;
            while (process_current != nullptr) {
                cout << "Process ID: " << process_current->process_id << " -> ";
                process_current = process_current->next;
            }
            cout << "NULL\n";
            current = current->next;
        }
    }

    void printStatus(int running_process_id) {
        cout << "Running: [" << running_process_id << "]\n";
        cout << "---------------------------\n";
        cout << "DQ: ";
        StackNode* current = top;
        while (current != nullptr) {
            ProcessNode* process_current = current->process_list;
            while (process_current != nullptr) {
                cout << process_current->process_id << (current == top ? "F" : "B") << " ";
                process_current = process_current->next;
            }
                        if (current->next != nullptr) {
                cout << "-> ";
            }
            current = current->next;
        }
        cout << "\n---------------------------\n";
    }
};

void shell(DynamicQueue &dq) {
    int process_id = 1;
    while (true) {
        unique_lock<mutex> lock(mtx);
        cout << "Shell: Adding foreground process with ID " << process_id << endl;
        dq.enqueue(process_id++, true);
        cv.wait_for(lock, chrono::seconds(1));
        if (stop) break;
    }
}

void monitor(DynamicQueue &dq) {
    int process_id = 100;
    while (true) {
        Sleep(2000); // Sleep for 2 seconds
        lock_guard<mutex> lock(mtx);
        cout << "Monitor: Adding background process with ID " << process_id << endl;
        dq.enqueue(process_id++, false);
        dq.display();
        if (stop) break;
    }
}

int main() {
    DynamicQueue dq;
    thread shell_thread(shell, ref(dq));
    thread monitor_thread(monitor, ref(dq));

    Sleep(10000); // Sleep for 10 seconds
    {
        lock_guard<mutex> lock(mtx);
        stop = true;
    }
    cv.notify_all();

    shell_thread.join();
    monitor_thread.join();

    while (ProcessNode* process = dq.dequeue()) {
        cout << "Dispatched process with ID " << process->process_id << endl;
        dq.printStatus(process->process_id);
        delete process;
        Sleep(1000); // Sleep for 1 second
    }

    cout << "===end of main()===\n";
    return 0;
}


