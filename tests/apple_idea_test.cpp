#include "../Source/AppleIdea.h"
#include "../Source/AIPieceSounds.h"

#include <cassert>

int main() {
    AppleIdea idea;
    idea.degrees = { 0, -1, 2, -1, 4, -1, 5, -1, 0, -1, 2, -1, 4, -1, 9, -1 };
    idea.noteCount = 8;
    assert(idea.isValid(0));
    const auto pentatonic = idea.midiPhrase(2, 0);
    assert(pentatonic[0] == 50);
    assert(pentatonic[1] == -1);
    assert(pentatonic[2] == 55);
    assert(pentatonic[14] == 72);

    idea.degrees[14] = 13;
    assert(!idea.isValid(0));
    assert(idea.isValid(1));
    const auto minor = idea.midiPhrase(2, 1);
    assert(minor[14] == 72);

    idea.degrees.fill(-1);
    assert(!idea.isValid(1));
    idea.degrees[0] = 0;
    idea.dynamics = 101;
    assert(!idea.isValid(1));

    idea.dynamics = 0;
    idea.noteCount = 1;
    assert(!idea.isValid(1));
    idea.noteCount = 4;
    idea.degrees.fill(0);
    assert(idea.isValid(0));
    const auto sparse = idea.spacedDegrees();
    assert(std::count_if(sparse.begin(), sparse.end(), [](int degree) { return degree >= 0; }) == 4);
    assert(sparse[0] == 0);
    idea.articulation = 0;
    idea.motion = 0;
    idea.texture = 0;
    idea.length = 0;
    idea.makePerformable();
    assert(idea.dynamics > 0 && idea.articulation > 0 && idea.motion > 0 && idea.texture > 0);
    assert(idea.length == 2);

    ApplePieceIdea piece;
    assert(piece.isValid());
    assert(!piece.hasMainMotif());
    piece.mainDegrees = { 0, -1, -1, -1, 2, -1, -1, -1, 4, -1, -1, -1, 2, -1, -1, -1 };
    assert(piece.hasMainMotif());
    const auto main = piece.mainPhrase();
    assert(main[0] >= 0 && main[1] == -1 && main[4] > main[0]);
    for (int role = 0; role < 4; ++role) {
        const auto follower = piece.followerPhrase(main, role, role);
        const int notes = static_cast<int>(std::count_if(follower.begin(), follower.end(), [](int note) { return note >= 0; }));
        assert(notes >= 1 && notes <= piece.mainNoteCount);
        assert(follower != main);
        if (role == 1 || role == 2 || role == 3)
            for (int step = 0; step < 16; ++step)
                assert(main[step] < 0 || follower[step] < 0);
    }
    assert(piece.targetTrackCount(0, 8) == 2);
    assert(piece.targetTrackCount(5, 8) == 5);
    assert(piece.targetTrackCount(8, 8) == 8);
    piece.trackCount = 8;
    assert(piece.isValid());
    assert(piece.targetTrackCount(2, 8) == 8);
    piece.trackCount = 2;
    piece.followerRoles = { 0, 0, 0 };
    assert(piece.roleForFollower(0) == 0);
    assert(piece.roleForFollower(1) != 0);
    assert(piece.roleForFollower(2) != piece.roleForFollower(0));
    assert(piece.roleForFollower(2) != piece.roleForFollower(1));
    for (int first = 0; first < 4; ++first)
        for (int second = 0; second < 4; ++second)
            for (int third = 0; third < 4; ++third) {
                piece.followerRoles = { first, second, third };
                assert(piece.roleForFollower(0) != piece.roleForFollower(1));
                assert(piece.roleForFollower(0) != piece.roleForFollower(2));
                assert(piece.roleForFollower(1) != piece.roleForFollower(2));
                for (int start : { 0, 4 }) {
                    std::array<bool, 4> roles{};
                    for (int follower = start; follower < start + 4; ++follower) {
                        const int role = piece.roleForFollower(follower);
                        assert(!roles[static_cast<size_t>(role)]);
                        roles[static_cast<size_t>(role)] = true;
                    }
                }
            }
    piece.trackCount = 9;
    assert(!piece.isValid());
    piece.trackCount = 3;
    piece.followerRoles = { 0, 2, 1 };
    assert(piece.isValid());
    piece.followerRoles[2] = 4;
    assert(!piece.isValid());
    piece.followerRoles[2] = 1;
    piece.form = 2;
    assert(!piece.isValid());
    piece.form = 1;
    piece.trackCount = 5;
    piece.mainDegrees.fill(-1);
    assert(piece.isValid() && piece.hasPlayablePlan() && !piece.hasMainMotif());
    piece.makePerformable();
    assert(piece.trackCount == 3 && piece.length == 3 && piece.articulation >= 65);
    piece.trackCount = 2;
    assert(piece.targetTrackCount(5, 8, 5) == 5);
    assert(piece.droneMutedCount(5) == 3);
    assert(piece.targetTrackCount(1, 8, 1) == 2);
    assert(piece.targetTrackCount(0, 8, 0) == 2);
    piece.trackCount = 3;
    for (int layer = 0; layer < 3; ++layer) {
        const auto drone = piece.dronePhrase(layer);
        assert(std::all_of(drone.begin(), drone.end(), [note = drone.front()](int value) { return value == note; }));
        assert(drone.front() >= 36 && drone.front() <= 71);
    }
    assert(piece.dronePhrase(0)[0] < piece.dronePhrase(1)[0]);
    const auto fallback = ApplePieceIdea::fallbackDrone(2);
    assert(fallback.isValid() && fallback.hasPlayablePlan());
    assert(fallback.form == 1 && fallback.trackCount == 2 && fallback.key == 2);
    assert(fallback.dronePhrase(0)[0] < fallback.dronePhrase(1)[0]);
    piece.form = 0;
    piece.mainDegrees = { 0, -1, -1, -1, 2, -1, -1, -1, 4, -1, -1, -1, 2, -1, -1, -1 };
    piece.trackCount = 3;
    piece.soundScene = 3;
    assert(!piece.isValid());
    piece.soundScene = 2;
    assert(piece.isValid());
    for (int palette = 0; palette < 4; ++palette) {
        const auto firstMain = AIPieceSounds::choose(palette, 0, 0, 0);
        const auto secondMain = AIPieceSounds::choose(palette, 1, 0, 0);
        assert(firstMain.kind != secondMain.kind || firstMain.sound != secondMain.sound);
        for (int scene = 0; scene < 3; ++scene) {
            for (int layer = 0; layer < 3; ++layer) {
                const auto droneSound = AIPieceSounds::chooseDrone(palette, scene, layer);
                assert(droneSound.kind == 1 && droneSound.sound >= 1 && droneSound.sound <= 13);
                assert(droneSound.level > 0 && droneSound.level <= 100);
            }
            const auto mainSound = AIPieceSounds::choose(palette, scene, 0, 0);
            assert(mainSound.kind == 1 || mainSound.kind == 2 || mainSound.kind == 4);
            assert(mainSound.level > 0 && mainSound.level <= 100);
            for (int role = 0; role < 4; ++role) {
                const auto followerSound = AIPieceSounds::choose(palette, scene, 1, role);
                assert(followerSound.kind == 1 || followerSound.kind == 2 || followerSound.kind == 4);
                assert(followerSound.sound > 0 && followerSound.sound <= (followerSound.kind == 2 ? 7 : followerSound.kind == 4 ? 4 : 13));
                assert(followerSound.level > 0 && followerSound.level < mainSound.level);
                assert(followerSound.kind != mainSound.kind || followerSound.sound != mainSound.sound);
                assert(followerSound.send >= 0 && followerSound.send <= 100);
                assert(followerSound.tail >= 0 && followerSound.tail <= 100);
                for (int other = 0; other < role; ++other) {
                    const auto earlier = AIPieceSounds::choose(palette, scene, 1, other);
                    assert(followerSound.kind != earlier.kind || followerSound.sound != earlier.sound);
                }
            }
        }
        const auto earlyFollower = AIPieceSounds::choose(palette, 0, 1, 0);
        const auto laterFollower = AIPieceSounds::choose(palette, 0, 4, 0);
        assert(earlyFollower.kind != laterFollower.kind || earlyFollower.sound != laterFollower.sound);
    }
    piece.density = 19;
    assert(!piece.isValid());
    piece.density = 30;
    piece.tempo = 19;
    assert(!piece.isValid());
    piece.tempo = 50;
    piece.palette = 0;
    piece.spaceReturn = 10;
    piece.roomSize = 45;
    piece.dynamics = piece.articulation = piece.motion = piece.texture = 0;
    piece.length = 0;
    piece.makePerformable();
    assert(piece.isValid());
    assert(piece.length == 2 && piece.dynamics > 0);
    assert(piece.spaceReturn >= 25 && piece.roomSize >= 60);
}
