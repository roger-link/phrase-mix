#pragma once

struct AIPieceSound {
    int kind;
    int sound;
    int level;
    int color;
    int send;
    int tail;
};

namespace AIPieceSounds {
    struct Choice { int kind, sound; };

    // Main, Shadow, Contrast, Sparse, Free. Later followers rotate scenes as roles repeat.
    constexpr Choice scenes[4][3][5] = {
        {{{1, 11}, {1, 5}, {2, 6}, {1, 2}, {1, 7}},
         {{1, 5}, {1, 4}, {2, 7}, {1, 12}, {1, 3}},
         {{1, 4}, {1, 11}, {1, 10}, {1, 2}, {1, 8}}},
        {{{1, 8}, {1, 5}, {2, 6}, {1, 2}, {1, 3}},
         {{1, 3}, {1, 4}, {1, 1}, {1, 12}, {1, 13}},
         {{1, 13}, {1, 5}, {1, 10}, {1, 2}, {1, 8}}},
        {{{2, 6}, {1, 10}, {1, 4}, {1, 12}, {1, 7}},
         {{2, 7}, {1, 4}, {1, 11}, {1, 2}, {1, 8}},
         {{1, 13}, {1, 1}, {2, 6}, {1, 12}, {1, 4}}},
        {{{1, 2}, {4, 2}, {1, 5}, {1, 12}, {1, 7}},
         {{1, 12}, {1, 4}, {4, 1}, {1, 2}, {1, 3}},
         {{1, 11}, {1, 7}, {2, 6}, {1, 2}, {1, 8}}}
    };

    constexpr Choice droneScenes[4][3][3] = {
        {{{1, 4}, {1, 5}, {1, 11}}, {{1, 5}, {1, 11}, {1, 4}}, {{1, 11}, {1, 4}, {1, 5}}},
        {{{1, 5}, {1, 13}, {1, 11}}, {{1, 13}, {1, 5}, {1, 4}}, {{1, 11}, {1, 13}, {1, 5}}},
        {{{1, 11}, {1, 5}, {1, 10}}, {{1, 10}, {1, 4}, {1, 11}}, {{1, 13}, {1, 11}, {1, 5}}},
        {{{1, 2}, {1, 7}, {1, 4}}, {{1, 12}, {1, 4}, {1, 7}}, {{1, 4}, {1, 2}, {1, 5}}}
    };

    constexpr int slotFor(int lane, int role) { return lane == 0 ? 0 : role + 1; }

    constexpr AIPieceSound choose(int palette, int scene, int lane, int role) {
        const int slot = slotFor(lane, role);
        const int followerScene = lane < 4 ? scene : (scene + 1 + (lane - 4) / 4) % 3;
        const Choice choice = scenes[palette][followerScene][slot];
        const bool low = slot == 3 || (choice.kind == 1 && (choice.sound == 2 || choice.sound == 12));
        const bool sustained = choice.kind == 4 || (choice.kind == 1 &&
            (choice.sound == 4 || choice.sound == 5 || choice.sound == 7 || choice.sound == 11));
        const bool struck = choice.kind == 2 || (choice.kind == 1 &&
            (choice.sound == 1 || choice.sound == 3 || choice.sound == 8 || choice.sound == 10 || choice.sound == 13));
        const int baseLevel[] = { 62, 47, 43, 42, 38 };
        return { choice.kind, choice.sound,
            baseLevel[slot] - (choice.kind == 2 ? 4 : 0) - (low ? 2 : 0),
            low ? 36 : struck ? 55 : sustained ? 43 : 48,
            low ? 24 : struck ? 33 : 48,
            low ? 20 : struck ? 10 : 34 };
    }

    constexpr AIPieceSound chooseDrone(int palette, int scene, int layer) {
        const int index = layer < 0 ? 0 : layer > 2 ? 2 : layer;
        const Choice choice = droneScenes[palette][scene][index];
        constexpr int levels[] = { 58, 40, 29 };
        constexpr int sends[] = { 36, 44, 52 };
        return { choice.kind, choice.sound, levels[index], 42, sends[index], 40 };
    }
}
