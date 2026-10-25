#pragma once

#include <array>
#include <cstddef>
#include <algorithm>

struct AppleIdea {
    std::array<int, 16> degrees{};
    int role = 0;
    int relationship = 35;
    int dynamics = 30;
    int articulation = 50;
    int motion = 35;
    int texture = 35;
    int length = 1;
    int noteCount = 6;

    bool isValid(int scale) const {
        bool hasNote = false;
        const int degreeCount = scale == 0 ? 10 : 14;
        for (const int degree : degrees) {
            if (degree < -1 || degree >= degreeCount) return false;
            hasNote |= degree >= 0;
        }
        const auto percentage = [](int value) { return value >= 0 && value <= 100; };
        return hasNote && noteCount >= 2 && noteCount <= 10 && role >= 0 && role <= 3 && length >= 0 && length <= 3
            && percentage(relationship) && percentage(dynamics) && percentage(articulation)
            && percentage(motion) && percentage(texture);
    }

    void makePerformable() {
        if (dynamics + articulation + motion + texture == 0) {
            dynamics = 22; articulation = 36; motion = 25; texture = 20;
        }
        if (noteCount <= 5 && length == 0) length = 2;
    }

    std::array<int, 16> spacedDegrees() const {
        auto spaced = degrees;
        int count = static_cast<int>(std::count_if(spaced.begin(), spaced.end(), [](int degree) { return degree >= 0; }));
        while (count > noteCount) {
            int remove = -1;
            int lowestScore = 100;
            for (int step = 0; step < 16; ++step) {
                if (spaced[step] < 0) continue;
                int score = step == 0 ? 8 : step % 4 == 0 ? 4 : step % 2 == 0 ? 2 : 0;
                if (spaced[(step + 15) % 16] >= 0) score -= 2;
                if (score < lowestScore) { lowestScore = score; remove = step; }
            }
            spaced[remove] = -1;
            --count;
        }
        return spaced;
    }

    std::array<int, 16> midiPhrase(int key, int scale) const {
        constexpr int pentatonic[] = { 0, 3, 5, 7, 10 };
        constexpr int minor[] = { 0, 2, 3, 5, 7, 8, 10 };
        constexpr int major[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int* tones = scale == 0 ? pentatonic : (scale == 1 ? minor : major);
        const int count = scale == 0 ? 5 : 7;
        const auto spaced = spacedDegrees();
        std::array<int, 16> phrase{};
        for (size_t step = 0; step < degrees.size(); ++step) {
            const int degree = spaced[step];
            phrase[step] = degree < 0 ? -1 : 48 + key + tones[degree % count] + 12 * (degree / count);
        }
        return phrase;
    }
};

struct ApplePieceIdea {
    ApplePieceIdea() { mainDegrees.fill(-1); }
    int form = 0;
    int trackCount = 2;
    std::array<int, 3> followerRoles{0, 2, 1};
    std::array<int, 16> mainDegrees{};
    int mainNoteCount = 4;
    int palette = 0;
    int soundScene = 0;
    int tempo = 80;
    int density = 50;
    int key = 0;
    int scale = 0;
    int shape = 0;
    int spaceReturn = 25;
    int roomSize = 60;
    int relationship = 35;
    int dynamics = 30;
    int articulation = 45;
    int motion = 35;
    int texture = 35;
    int length = 1;

    static ApplePieceIdea fallbackDrone(int currentKey) {
        ApplePieceIdea idea;
        idea.form = 1;
        idea.trackCount = 2;
        idea.palette = 0;
        idea.soundScene = 1;
        idea.tempo = 52;
        idea.density = 30;
        idea.key = std::clamp(currentKey, 0, 11);
        idea.scale = 0;
        idea.spaceReturn = 34;
        idea.roomSize = 68;
        idea.relationship = 35;
        idea.dynamics = 25;
        idea.articulation = 78;
        idea.motion = 38;
        idea.texture = 40;
        idea.length = 3;
        return idea;
    }

    bool isValid() const {
        const auto between = [](int value, int low, int high) { return value >= low && value <= high; };
        return between(form, 0, 1) && between(trackCount, 1, 8) && between(palette, 0, 3) && between(soundScene, 0, 2)
            && std::all_of(followerRoles.begin(), followerRoles.end(), [](int role) { return role >= 0 && role <= 3; })
            && between(tempo, 20, 180) && between(density, 20, 80) && between(key, 0, 11)
            && between(scale, 0, 2) && between(shape, 0, 5)
            && between(spaceReturn, 0, 100) && between(roomSize, 0, 100)
            && between(relationship, 0, 100) && between(dynamics, 0, 100)
            && between(articulation, 0, 100) && between(motion, 0, 100)
            && between(texture, 0, 100) && between(length, 0, 3);
    }

    void makePerformable() {
        if (dynamics + articulation + motion + texture == 0) {
            dynamics = 22; articulation = 36; motion = 25; texture = 20;
        }
        if (density <= 40 && length == 0) length = 2;
        if ((palette == 0 || palette == 3) && tempo <= 65) {
            spaceReturn = std::max(spaceReturn, 25);
            roomSize = std::max(roomSize, 60);
            length = std::max(length, 2);
        }
        if (form == 1) {
            trackCount = std::min(trackCount, 3);
            dynamics = std::min(dynamics, 30);
            articulation = std::max(articulation, 65);
            motion = std::max(motion, 28);
            texture = std::max(texture, 25);
            length = 3;
        }
    }

    bool hasPlayablePlan() const { return form == 1 || hasMainMotif(); }

    std::array<int, 16> dronePhrase(int layer) const {
        std::array<int, 16> phrase{};
        constexpr int intervals[] = { 0, 7, 12 };
        const int root = (palette == 3 ? 36 : 48) + key;
        phrase.fill(root + intervals[std::clamp(layer, 0, 2)]);
        return phrase;
    }

    bool hasMainMotif() const {
        AppleIdea motif;
        motif.degrees = mainDegrees;
        motif.noteCount = mainNoteCount;
        return mainNoteCount >= 2 && mainNoteCount <= 8 && motif.isValid(scale);
    }

    std::array<int, 16> mainPhrase() const {
        AppleIdea motif;
        motif.degrees = mainDegrees;
        motif.noteCount = mainNoteCount;
        return motif.midiPhrase(key, scale);
    }

    std::array<int, 16> followerPhrase(const std::array<int, 16>& main, int role, int follower) const {
        std::array<int, 16> phrase;
        phrase.fill(-1);
        const int shift = role == 0 ? 2 : role == 1 ? 3 : role == 2 ? 6 : 5;
        const int transpose = role == 0 || role == 2 ? -12 : role == 1 ? 7 : 12;
        int active = 0;
        for (int source = 0; source < 16; ++source) {
            if (main[source] < 0) continue;
            if ((role == 0 || role == 2) && active++ % 2 != 0) continue;
            int step = (source + shift + 2 * (follower / 3)) % 16;
            if (role != 0) {
                for (int tries = 0; tries < 16 && (main[step] >= 0 || phrase[step] >= 0); ++tries)
                    step = (step + 1) % 16;
            }
            int note = main[source] + transpose;
            while (note < 36) note += 12;
            while (note > 84) note -= 12;
            phrase[step] = note;
        }
        return phrase;
    }

    int targetTrackCount(int current, int limit) const {
        return std::min(limit, std::max(current, trackCount));
    }

    int targetTrackCount(int current, int limit, int availableUnheld) const {
        if (form == 1) return std::min(limit, current + std::max(0, trackCount - std::max(0, availableUnheld)));
        return targetTrackCount(current, limit);
    }

    int droneMutedCount(int availableUnheld) const {
        return form == 1 ? std::max(0, availableUnheld - trackCount) : 0;
    }

    int roleForFollower(int follower) const {
        if (follower >= 4) return roleForFollower(follower % 4);
        constexpr std::array<int, 4> fallback{0, 2, 1, 3};
        int role = followerRoles[static_cast<size_t>(follower % 3)];
        std::array<bool, 4> used{};
        for (int earlier = 0; earlier < follower && earlier < 3; ++earlier)
            used[static_cast<size_t>(roleForFollower(earlier))] = true;
        if (used[static_cast<size_t>(role)]) {
            for (int candidate : fallback)
                if (!used[static_cast<size_t>(candidate)]) { role = candidate; break; }
        }
        return role;
    }
};
