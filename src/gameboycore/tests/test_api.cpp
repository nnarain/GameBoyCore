#include <gtest/gtest.h>

#include "test_helper.h"
#include "util/codegenerator.h"

#include <gameboycore/gameboycore.h>
#include <gameboycore/memorymap.h>
#include <gameboycore/instruction.h>

#include <vector>

using namespace gb;

TEST(API, SetBeforeLoad)
{
    CodeGenerator code;
    code.block(
        0xF0, 0x44,       // LDH A, ($44)
        0xFE, 0x90,       // CP A, $90    -> CP A, 144
        0xC2, 0x50, 0x01, // JP NZ, $150
        0x76              // HALT
    );

    GameboyCore core;

    bool vblank_fired = false;
    core.setVBlankCallback([&vblank_fired](){ vblank_fired = true; });

    auto rom = code.rom();
    core.loadROM(&rom[0], rom.size());

    core.emulateFrame();

    EXPECT_TRUE(vblank_fired);
}

TEST(API, CallbackAfterReset)
{
    CodeGenerator code;
    code.block(
        0xF0, 0x44,       // LDH A, ($44)
        0xFE, 0x90,       // CP A, $90    -> CP A, 144
        0xC2, 0x50, 0x01, // JP NZ, $150
        0x76              // HALT
    );

    GameboyCore core;

    bool vblank_fired = false;
    core.setVBlankCallback([&vblank_fired]() { vblank_fired = true; });

    auto rom = code.rom();
    core.loadROM(&rom[0], rom.size());

    core.emulateFrame();

    EXPECT_TRUE(vblank_fired);

    // run again to show the callback stays after a core reset
    core.loadROM(&rom[0], rom.size());

    core.emulateFrame();

    EXPECT_TRUE(vblank_fired);
}

TEST(API, SpriteAttributes)
{
    /**
        OAM Registers - 4 bytes per sprite

        0 - y coord
        1 - x coord
        2 - CHR code (tile number)
        3 - Attributes
            Bits:
                7     - Priority
                6     - Vertical flip
                5     - Horizontal flip
                4     - DMG palette
                3     - CBG character bank
                2,1,0 - CGB palette
    */

    CodeGenerator code;
    code.block(
        0x3E, 0x82,      // LD A, $82    ; LCDC enable | sprites enabled
        0xE0, 0x40,      // LDH ($40), A
        0x3E, 0x20,      // LD A, $20    ; For DMA, from $2000
        0xE0, 0x46,      // LDH ($46), A
        0xC3, 0x58, 0x01 // JP $0158     ; Loop
    );

    // sprite data
    code.address(0x2000);
    code.block(
        0x08, 0x08, 0x01, 0x20, // horizontal flip
        0x00, 0x00, 0x02, 0x40, // vertical flip
        0x00, 0x00, 0x03, 0x80, // display priority
        0x00, 0x00, 0x00, 0x07, // CGB palette 7
        0x00, 0x00, 0x00, 0x10  // DMG palette
    );

    // initialize the core
    GameboyCore core;

    auto rom = code.rom();
    core.loadROM(&rom[0], rom.size());

    // emulate a frame to populate sprite cache
    core.emulateFrame();

    // get sprites
    auto sprites = core.getGPU()->getSpriteCache();

    const auto& s0 = sprites[0];
    const auto& s1 = sprites[1];
    const auto& s2 = sprites[2];
    const auto& s3 = sprites[3];
    const auto& s4 = sprites[4];

    EXPECT_EQ(s0.x, 8);
    EXPECT_EQ(s0.y, 8);
    EXPECT_EQ(s0.tile, 1);
    EXPECT_EQ(s0.isHorizontallyFlipped(), true);
    EXPECT_EQ(s0.isVerticallyFlipped(), false);

    EXPECT_EQ(s1.isHorizontallyFlipped(), false);
    EXPECT_EQ(s1.isVerticallyFlipped(), true);

    EXPECT_EQ(!s2.hasPriority(), true);

    EXPECT_EQ(s3.getCgbPalette(), 0x07);

    EXPECT_EQ(s4.paletteOBP0(), 1);
}

TEST(API, GetAndSetBatteryRAM)
{
    CodeGenerator code;
    code.block(0x76);

    GameboyCore core;
    core.loadROM(code.rom());

    // enable xram by writing $0A into ROM < $1FFF
    core.writeMemory(0x1000, 0x0A);

    // Write a value into battery ram
    core.writeMemory(0xA000, 0xDE);
    core.writeMemory(0xBFFF, 0xCC);

    // Get battery RAM
    auto batteryram = core.getBatteryRam();

    EXPECT_EQ(batteryram[0], 0xDE);
    EXPECT_EQ(batteryram[batteryram.size() - 1], 0xCC);

    batteryram[0] = 0xAD;
    batteryram[batteryram.size() - 1] = 0xBB;
    core.setBatteryRam(batteryram);

    auto value1 = core.readMemory(0xA000);
    auto value2 = core.readMemory(0xBFFF);

    EXPECT_EQ(value1, 0xAD);
    EXPECT_EQ(value2, 0xBB);
}

TEST(API, Serialization)
{
    CodeGenerator code{ CodeGenerator::MBC::MBC1 };
    code.block(
        0x3E, 0x0A, // LD A, $0A
        0x06, 0x0B, // LD B, $0B
        0x0E, 0x0C, // LD C, $0C
        0x16, 0x0D, // LD D, $0D
        0x1E, 0x0E, // LD E, $0E
        0x26, 0x0F, // LD H, $0F
        0x2E, 0x09  // LD L, $09
    );

    GameboyCore core;
    core.loadROM(code.rom());

    // Enable XRAM
    core.writeMemory(0x1000, 0x0A);
    // Write values into RAM
    core.writeMemory(0x8000, 0xDE);
    core.writeMemory(0xA000, 0xAD);

    // Run the core for the necessary number of steps
    core.update(9);

    auto first_state = core.getCPU()->getStatus();

    EXPECT_EQ(first_state.a, 0x0A);
    EXPECT_EQ(first_state.b, 0x0B);
    EXPECT_EQ(first_state.c, 0x0C);
    EXPECT_EQ(first_state.d, 0x0D);
    EXPECT_EQ(first_state.e, 0x0E);
    EXPECT_EQ(first_state.h, 0x0F);
    EXPECT_EQ(first_state.l, 0x09);
    EXPECT_EQ(core.readMemory(0x8000), 0xDE);
    EXPECT_EQ(core.readMemory(0xA000), 0xAD);

    const auto data = core.serialize();
    core.deserialize(data);

    auto second_state = core.getCPU()->getStatus();

    EXPECT_EQ(second_state.a, 0x0A);
    EXPECT_EQ(second_state.b, 0x0B);
    EXPECT_EQ(second_state.c, 0x0C);
    EXPECT_EQ(second_state.d, 0x0D);
    EXPECT_EQ(second_state.e, 0x0E);
    EXPECT_EQ(second_state.h, 0x0F);
    EXPECT_EQ(second_state.l, 0x09);
    EXPECT_EQ(core.readMemory(0x8000), 0xDE);
    EXPECT_EQ(core.readMemory(0xA000), 0xAD);
}

TEST(API, InstructionCallback)
{
    std::vector<gb::Instruction> instructions;
    const auto instruction_callback = [&instructions](const gb::Instruction & instr, const uint16_t addr) {instructions.push_back(instr); };

    CodeGenerator code;
    code.block(
        0x3E, 0x0A // LD A, $0A
    );

    GameboyCore core;
    core.loadROM(code.rom());

    core.setInstructionCallback(instruction_callback);
    core.setDebugMode(true);

    core.update(3);

    EXPECT_EQ(instructions.size(), 3);
    
    EXPECT_EQ(instructions[0].opcode, 0x00);

    EXPECT_EQ(instructions[1].opcode, 0xC3);
    EXPECT_EQ(instructions[1].operand_data[0], 0x50);
    EXPECT_EQ(instructions[1].operand_data[1], 0x01);

    EXPECT_EQ(instructions[2].opcode, 0x3E);
    EXPECT_EQ(instructions[2].operand_data[0], 0x0A);
    EXPECT_EQ(instructions[2].operand_data[1], 0x00);
}
