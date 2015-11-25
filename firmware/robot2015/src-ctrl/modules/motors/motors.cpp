#include "motors.hpp"

#include <mbed.h>
#include <rtos.h>
#include <Console.hpp>
#include <numparser.hpp>

#include "fpga.hpp"
#include "commands.hpp"

motor_t mtrEx = {
    .targetVel = 0x4D,
    .adjVel = 0x64,
    .hallCount = 0x0A,
    .encCounts = {0x23, 0x18},
    .status = {.encOK = false, .hallOK = true, .drvStatus = {0x26, 0x0F}},
    .desc = "Motor "};

std::array<motor_t, 5> motors;

void motors_Init() {
    // ahhh just make a dummy vector of info for now...
    motors.fill(mtrEx);
    motors.at(0).desc = "Dribbler";
    motors.at(1).desc += "1";
    motors.at(2).desc += "2";
    motors.at(3).desc += "3";
    motors.at(4).desc += "4";
}

// An easy-to-read printf/logging function for a single motor
std::ostream& cmd_motors_print(std::ostream& s, const motor_t& mtr) {
    s << mtr.desc << endl
      << "  Target Vel:\t\t" << mtr.targetVel << endl
      << "  Adjusted Vel:\t\t" << mtr.adjVel << endl
      << "  Hall Cnt:\t\t" << mtr.hallCount << endl
      << "  Encoder Cnt:\t\t" << mtr.encCounts[0]
      << "\t[prev: " << mtr.encCounts[1] << "]" << endl
      << "  Enc OK:\t\t" << (mtr.status.encOK ? "YES" : "NO") << endl
      << "  Hall OK:\t\t" << (mtr.status.hallOK ? "YES" : "NO") << endl
      << "  DRV Addr 0x00:\t0x" << std::hex << mtr.status.drvStatus[0] << endl
      << "  DRV Addr 0x01:\t0x" << std::hex << mtr.status.drvStatus[1] << endl;
    return s;
}

ostream& cmd_motors_scroll(ostream& s, const std::vector<std::string>& args) {
    std::array<uint16_t, 5> duty_cycles = {0};
    std::array<uint8_t, 5> halls = {0};
    std::array<uint16_t, 5> enc_deltas = {0};

    FPGA::Instance()->read_duty_cycles(duty_cycles.data(), duty_cycles.size());
    FPGA::Instance()->read_halls(halls.data(), halls.size());
    uint8_t status_byte =
        FPGA::Instance()->read_encs(enc_deltas.data(), enc_deltas.size());

    s << "\033[?25l\033[25mMotors Enabled: \033[K"
      << (status_byte & 0x20 ? "YES" : "NO") << "\033E";

    for (size_t i = 0; i < duty_cycles.size(); i++) {
        s << "  Motor " << i + 1 << "\tVel: 0x\033[K" << duty_cycles.at(i)
          << "\tHall: " << halls.at(i) << "\tEnc: " << enc_deltas.at(i)
          << ((status_byte & (1 << i)) ? "FAULT" : "") << "\033E";
    }
    s << "\033[" << (duty_cycles.size() + 1) << "A\033[?25h";

    Console::Flush();
    Thread::wait(250);

    return s;
}

// The console function to run with the 'motor' command
ostream& cmd_motors(ostream& s, const vector<string>& args) {
    if (args.empty() == true) {
        s << "Must specify a motor ID!" << endl;
    } else {
        // Default to displaying motor info
        std::vector<uint8_t> motorIDs;

        // Check for valid motor ID arguments
        for (unsigned int i = 0; i < args.size(); i++) {
            std::string mtrArg = args.at(i);

            if (isInt(mtrArg)) {
                // Get the string argument into a type we can work with
                uint8_t mtrID = (uint8_t)atoi(mtrArg.c_str());

                // they gave us an invalid motor ID...
                if (mtrID > 4) break;

                // Push the ID into the vector if it's not already in it
                if (std::find(motorIDs.begin(), motorIDs.end(), mtrID) ==
                    motorIDs.end())
                    motorIDs.push_back(mtrID);
            }
        }

        if (motorIDs.empty() == false) {
            if (strcmp(args.at(motorIDs.size()).c_str(), "--set") == 0 ||
                strcmp(args.at(motorIDs.size()).c_str(), "-s") == 0) {
                std::array<uint16_t, 5> duty_cycles_r;
                std::array<uint16_t, 5> duty_cycles_w;
                std::array<uint8_t, 5> halls;
                std::array<uint16_t, 5> enc_deltas;

                uint16_t new_vel =
                    (uint16_t)atoi(args.at(motorIDs.size() + 1).c_str());

                FPGA::Instance()->read_duty_cycles(duty_cycles_r.data(),
                                                   duty_cycles_r.size());
                std::memcpy(duty_cycles_w.data(), duty_cycles_r.data(),
                            duty_cycles_r.size());

                FPGA::Instance()->read_halls(halls.data(), halls.size());

                for (unsigned int i = 0; i < motorIDs.size(); i++)
                    duty_cycles_w.at(i) = new_vel;

                uint8_t status_byte = FPGA::Instance()->set_duty_get_enc(
                    duty_cycles_w.data(), duty_cycles_w.size(),
                    enc_deltas.data(), enc_deltas.size());

                for (unsigned int i = 0; i < motorIDs.size(); i++) {
                    s << "  Motor " << motorIDs.at(i) << "\tVel: 0x\033[K"
                      << duty_cycles_r.at(motorIDs.at(i)) << " -> 0x"
                      << duty_cycles_w.at(motorIDs.at(i))
                      << "\tHall: " << halls.at(motorIDs.at(i))
                      << "\tEnc: " << enc_deltas.at(motorIDs.at(i))
                      << (status_byte & (1 << motorIDs.at(i)) ? "FAULT" : "")
                      << "\033E";
                }

            } else {
                // If we make it to this point, all arguments given are valid
                // motor ID numbers - without duplicate entries
                for (unsigned int i = 0; i < motorIDs.size(); i++)
                    s << cmd_motors_print(s, motors.at(motorIDs.at(i)));
            }

        } else {
            s << showInvalidArgs(s, args.at(0));
        }
    }
    return s;
}
