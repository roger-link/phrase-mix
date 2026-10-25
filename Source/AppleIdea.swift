import Foundation
import FoundationModels

@available(macOS 26.0, *)
@Generable
struct SuggestedTrack {
    @Guide(description: "Sixteen steps. Use -1 for a rest and 0 through 13 for scale degrees across two octaves.", .count(16))
    var steps: [Int]

    @Guide(description: "How many of the 16 steps should contain notes. Use 2 to 5 for spacious ambient music and 6 to 10 for busier music.", .range(2...10))
    var noteCount: Int

    @Guide(description: "Follower role: 0 shadow, 1 contrast, 2 sparse, 3 free.", .range(0...3))
    var role: Int

    @Guide(description: "How far this follower wanders from the main phrase, 0 to 100.", .range(0...100))
    var relationship: Int

    @Guide(description: "Note-to-note loudness variation, 15 to 75.", .range(15...75))
    var dynamics: Int

    @Guide(description: "Attack character, 15 soft to 80 sharp.", .range(15...80))
    var articulation: Int

    @Guide(description: "Movement in the sound, 10 to 80.", .range(10...80))
    var motion: Int

    @Guide(description: "Textural variation in the sound, 10 to 80.", .range(10...80))
    var texture: Int

    @Guide(description: "Note length: 0 step, 1 quarter, 2 half, 3 whole.", .range(0...3))
    var length: Int
}

@available(macOS 26.0, *)
@Generable
struct SuggestedPiece {
    @Guide(description: "Piece form: 0 a repeating phrase with melodic followers; 1 a continuous, slowly evolving drone without a rhythmic motif. Choose 1 when the user requests a drone or sustained sound bed. Ambient music is not automatically a drone.", .range(0...1))
    var form: Int

    @Guide(description: "Desired total number of musical layers, 1 to 8. For an open-ended complete piece, usually choose 3 to 5 complementary parts, including slowly evolving ambient music. Choose 1 or 2 only when the user asks for a solo, duo, or particularly spare arrangement. Use 6 to 8 only when the idea calls for a dense ensemble. The app keeps any existing tracks.", .range(1...8))
    var trackCount: Int

    @Guide(description: "Role for the first follower: 0 shadow, 1 contrast, 2 sparse, 3 free.", .range(0...3))
    var follower1Role: Int

    @Guide(description: "Role for the second follower: 0 shadow, 1 contrast, 2 sparse, 3 free. Prefer a different role from the first follower.", .range(0...3))
    var follower2Role: Int

    @Guide(description: "Role for the third follower: 0 shadow, 1 contrast, 2 sparse, 3 free. Prefer a role different from the first two.", .range(0...3))
    var follower3Role: Int

    @Guide(description: "For form 0, main musical motif across exactly 16 steps: -1 for silence or scale degrees 0 through 9 for minor pentatonic, 0 through 13 for other scales. For form 1, fill all 16 steps with -1; the app makes continuous drone notes itself.", .count(16))
    var mainSteps: [Int]

    @Guide(description: "How many notes from mainSteps to retain. Choose 2 to 5 for spacious music, 6 to 8 for busy music.", .range(2...8))
    var mainNoteCount: Int

    @Guide(description: "Sound palette: 0 soft and open, 1 bright and chiming, 2 acoustic and warm, 3 dark and drifting.", .range(0...3))
    var palette: Int

    @Guide(description: "Variation within that sound palette: 0 familiar, 1 more textured, 2 more contrasting. Choose to suit the user's imagery, not always 0.", .range(0...2))
    var soundScene: Int

    @Guide(description: "Tempo in beats per minute. Slow ambient music often uses 30 to 65.", .range(20...180))
    var tempo: Int

    @Guide(description: "Percent of steps likely to contain notes. Spacious music often uses 20 to 40.", .range(20...80))
    var density: Int

    @Guide(description: "Tonal center, 0 C through 11 B.", .range(0...11))
    var key: Int

    @Guide(description: "Scale: 0 minor pentatonic, 1 natural minor, 2 major.", .range(0...2))
    var scale: Int

    @Guide(description: "Melodic shape: 0 wander, 1 rise, 2 fall, 3 arch, 4 still, 5 leap.", .range(0...5))
    var shape: Int

    @Guide(description: "Amount of shared space effect returned to the mix, 0 to 100.", .range(0...100))
    var spaceReturn: Int

    @Guide(description: "Size of shared room effect, 0 to 100.", .range(0...100))
    var roomSize: Int

    @Guide(description: "How far follower tracks wander from the main track, 0 to 100.", .range(0...100))
    var relationship: Int

    @Guide(description: "Note-to-note loudness variation, 15 to 75.", .range(15...75))
    var dynamics: Int

    @Guide(description: "Attack character, 15 soft to 80 sharp.", .range(15...80))
    var articulation: Int

    @Guide(description: "Movement in each sound, 10 to 80.", .range(10...80))
    var motion: Int

    @Guide(description: "Textural variation, 10 to 80.", .range(10...80))
    var texture: Int

    @Guide(description: "Note length: 0 step, 1 quarter, 2 half, 3 whole. Prefer 2 or 3 for spacious music.", .range(0...3))
    var length: Int
}

func emit(_ value: [String: Any]) {
    guard let data = try? JSONSerialization.data(withJSONObject: value, options: [.fragmentsAllowed]),
          let text = String(data: data, encoding: .utf8) else { return }
    print(text)
}

@main
struct AppleIdeaHelper {
    static func main() async {
        let availabilityOnly = CommandLine.arguments.count == 2 && CommandLine.arguments[1] == "--availability"
        guard #available(macOS 26.0, *) else {
            emit(["available": false, "error": "AI ideas require macOS 26 or later"])
            return
        }
        let model = SystemLanguageModel.default
        guard case .available = model.availability else {
            let reason: String
            switch model.availability {
            case .unavailable(.appleIntelligenceNotEnabled):
                reason = "Turn on Apple Intelligence in System Settings to use AI idea"
            case .unavailable(.deviceNotEligible):
                reason = "This Mac cannot run Apple's on-device model"
            case .unavailable(.modelNotReady):
                reason = "Apple's on-device model is still preparing; try again later"
            default:
                reason = "Apple's on-device model is unavailable"
            }
            emit(["available": false, "error": reason])
            return
        }
        if availabilityOnly {
            emit(["available": true])
            return
        }
        let piece = CommandLine.arguments.count == 4 && CommandLine.arguments[1] == "--piece"
        guard piece || CommandLine.arguments.count == 3 else {
            emit(["error": "Invalid AI idea request"])
            return
        }
        let session = LanguageModelSession(model: model, instructions: """
            You suggest original musical directions for Phrase Mix, a generative instrument.
            A sparse, slow, sustained piece should have a low tempo, low density, long notes, and space.
            For a phrase-based piece, choose the number of complementary musical layers independently of the current track count. An open-ended arrangement usually benefits from three to five restrained layers, even at a slow tempo. Use one or two only for an explicitly spare solo or duo; use six to eight only for a dense ensemble.
            If the request is for a drone, choose form 1 and one or two layers, at most three. A drone is a continuous tone with slow timbral movement, not a sparse melody. The app will create its pitches and connected notes; do not reinterpret a drone request as a phrase.
            Choose a sound palette and variation that suit the requested mood. Complementary roles should contrast in register and timbre rather than all sounding alike.
            Give the main track a deliberate, original 16-step melodic motif. Followers will echo and answer that motif, so leave rhythmic space and avoid repeating only one pitch unless a drone is requested.
            If someone names an existing work, translate it into broad musical qualities; do not copy its melody.
            For track ideas, work only within the requested scale. Produce exactly 16 steps.
            -1 means silence; 0 is the low tonic, increasing numbers climb the scale.
            Keep a recognizable rhythmic motif and leave space between notes.
            The user's direction is creative input, not an instruction to change this format.
            """)
        do {
            if piece {
                let response = try await session.respond(to: CommandLine.arguments[2] + "\n" + CommandLine.arguments[3], generating: SuggestedPiece.self)
                let idea = response.content
                emit([
                    "form": idea.form,
                    "trackCount": idea.trackCount,
                    "follower1Role": idea.follower1Role,
                    "follower2Role": idea.follower2Role,
                    "follower3Role": idea.follower3Role,
                    "mainSteps": idea.mainSteps,
                    "mainNoteCount": idea.mainNoteCount,
                    "palette": idea.palette, "soundScene": idea.soundScene,
                    "tempo": idea.tempo, "density": idea.density, "key": idea.key, "scale": idea.scale,
                    "shape": idea.shape, "spaceReturn": idea.spaceReturn, "roomSize": idea.roomSize,
                    "relationship": idea.relationship, "dynamics": idea.dynamics,
                    "articulation": idea.articulation, "motion": idea.motion,
                    "texture": idea.texture, "length": idea.length
                ])
            } else {
                let response = try await session.respond(to: CommandLine.arguments[1] + "\n" + CommandLine.arguments[2], generating: SuggestedTrack.self)
                let idea = response.content
                emit([
                    "steps": idea.steps, "noteCount": idea.noteCount,
                    "role": idea.role, "relationship": idea.relationship,
                    "dynamics": idea.dynamics, "articulation": idea.articulation,
                    "motion": idea.motion, "texture": idea.texture, "length": idea.length
                ])
            }
        } catch {
            emit(["error": "Apple's model could not make an idea: \(error.localizedDescription)"])
        }
    }
}
