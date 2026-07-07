#include <iostream>

// הצהרות קדימה (Forward Declarations) - חוסך קובצי .h לטסטים
void run_parser_tests();
void run_commands_tests();
void run_pieces_tests();
void run_realtime_tests(); 

int main() {
    std::cout << "--- Executing All Test Suites ---\n";
    run_parser_tests();
    run_commands_tests();
    run_pieces_tests();
    run_realtime_tests(); 
    return 0;
}