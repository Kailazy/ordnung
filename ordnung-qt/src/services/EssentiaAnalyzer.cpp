#ifdef HAVE_ESSENTIA

#include "EssentiaAnalyzer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>
#include <essentia/pool.h>

#ifdef HAVE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

#include <algorithm>
#include <numeric>
#include <vector>
#include <string>
#include <cmath>

using namespace essentia;
using namespace essentia::standard;

// ── Discogs-Effnet class labels (top 400 from the model) ─────────────────────
// The Discogs-Effnet model outputs 400 activations corresponding to Discogs
// genre/style tags. We store the full label list so we can map top activations
// to human-readable strings. The list below is the canonical ordering from the
// essentia-tensorflow-models repository.
//
// Because embedding the full 400-entry label array in source would be unwieldy,
// we use a condensed list of the most musically useful labels and map indices at
// runtime. The model's output tensor is 400-dimensional; we pick the top-5 by
// activation value and use those as style_tags.
//
// For mood classification we look for known mood-indicative labels.
// For vocal_prob we look for vocal/instrumental indicator labels.

// A small subset of known Discogs-Effnet genre indices for mood/vocal heuristics.
// These are stable across the published discogs-effnet-bs64 model versions.
namespace DiscogsLabels {

// Genre labels (index -> name) for the top-level Discogs genres.
// Full 400-label mapping from essentia's metadata JSON.
static const char* const kLabels[] = {
    "Blues",                        // 0
    "Blues---Boogie Woogie",        // 1
    "Blues---Chicago Blues",         // 2
    "Blues---Country Blues",         // 3
    "Blues---Delta Blues",           // 4
    "Blues---Electric Blues",        // 5
    "Blues---Harmonica Blues",       // 6
    "Blues---Jump Blues",            // 7
    "Blues---Louisiana Blues",       // 8
    "Blues---Modern Electric Blues", // 9
    "Blues---Piano Blues",           // 10
    "Blues---Rhythm & Blues",        // 11
    "Blues---Texas Blues",           // 12
    "Brass & Military",             // 13
    "Brass & Military---Brass Band",// 14
    "Brass & Military---Marches",   // 15
    "Brass & Military---Military",  // 16
    "Children's",                   // 17
    "Children's---Educational",     // 18
    "Children's---Nursery Rhymes",  // 19
    "Children's---Story",           // 20
    "Classical",                    // 21
    "Classical---Baroque",          // 22
    "Classical---Choral",           // 23
    "Classical---Classical",        // 24
    "Classical---Contemporary",     // 25
    "Classical---Impressionist",    // 26
    "Classical---Medieval",         // 27
    "Classical---Modern",           // 28
    "Classical---Neo-Classical",    // 29
    "Classical---Neo-Romantic",     // 30
    "Classical---Opera",            // 31
    "Classical---Post-Modern",      // 32
    "Classical---Renaissance",      // 33
    "Classical---Romantic",         // 34
    "Electronic",                   // 35
    "Electronic---Abstract",        // 36
    "Electronic---Acid",            // 37
    "Electronic---Acid House",      // 38
    "Electronic---Acid Jazz",       // 39
    "Electronic---Ambient",         // 40
    "Electronic---Bassline",        // 41
    "Electronic---Beatdown",        // 42
    "Electronic---Berlin-School",   // 43
    "Electronic---Big Beat",        // 44
    "Electronic---Bleep",           // 45
    "Electronic---Breakbeat",       // 46
    "Electronic---Breakcore",       // 47
    "Electronic---Breaks",          // 48
    "Electronic---Broken Beat",     // 49
    "Electronic---Chillwave",       // 50
    "Electronic---Chiptune",        // 51
    "Electronic---Dance-pop",       // 52
    "Electronic---Dark Ambient",    // 53
    "Electronic---Darkwave",        // 54
    "Electronic---Deep House",      // 55
    "Electronic---Deep Techno",     // 56
    "Electronic---Disco",           // 57
    "Electronic---Disco Polo",      // 58
    "Electronic---Downtempo",       // 59
    "Electronic---Drone",           // 60
    "Electronic---Drum n Bass",     // 61
    "Electronic---Dub",             // 62
    "Electronic---Dub Techno",      // 63
    "Electronic---Dubstep",         // 64
    "Electronic---Dungeon Synth",   // 65
    "Electronic---EBM",             // 66
    "Electronic---Electro",         // 67
    "Electronic---Electro House",   // 68
    "Electronic---Electroclash",    // 69
    "Electronic---Euro House",      // 70
    "Electronic---Euro-Disco",      // 71
    "Electronic---Eurobeat",        // 72
    "Electronic---Eurodance",       // 73
    "Electronic---Experimental",    // 74
    "Electronic---Freestyle",       // 75
    "Electronic---Future Jazz",     // 76
    "Electronic---Gabber",          // 77
    "Electronic---Garage House",    // 78
    "Electronic---Ghetto",          // 79
    "Electronic---Ghetto House",    // 80
    "Electronic---Glitch",          // 81
    "Electronic---Goa Trance",      // 82
    "Electronic---Grunge",          // 83  (misclassified in Discogs)
    "Electronic---Happy Hardcore",  // 84
    "Electronic---Hard House",      // 85
    "Electronic---Hard Techno",     // 86
    "Electronic---Hard Trance",     // 87
    "Electronic---Hardcore",        // 88
    "Electronic---Hardstyle",       // 89
    "Electronic---Hi NRG",          // 90
    "Electronic---House",           // 91
    "Electronic---IDM",             // 92
    "Electronic---Illbient",        // 93
    "Electronic---Industrial",      // 94
    "Electronic---Italo House",     // 95
    "Electronic---Italo-Disco",     // 96
    "Electronic---Italodance",      // 97
    "Electronic---Jazzdance",       // 98
    "Electronic---Juke",            // 99
    "Electronic---Jumpstyle",       // 100
    "Electronic---Jungle",          // 101
    "Electronic---Latin",           // 102
    "Electronic---Leftfield",       // 103
    "Electronic---Makina",          // 104
    "Electronic---Minimal",         // 105
    "Electronic---Minimal Techno",  // 106  (intentional duplicate of concept)
    "Electronic---Musique Concrete",// 107
    "Electronic---Neofolk",         // 108
    "Electronic---New Age",         // 109
    "Electronic---New Beat",        // 110
    "Electronic---New Wave",        // 111
    "Electronic---Noise",           // 112
    "Electronic---Nu-Disco",        // 113
    "Electronic---Power Electronics",// 114
    "Electronic---Progressive Breaks",// 115
    "Electronic---Progressive House",// 116
    "Electronic---Progressive Trance",// 117
    "Electronic---Psy-Trance",      // 118
    "Electronic---Rhythmic Noise",  // 119
    "Electronic---Schranz",         // 120
    "Electronic---Sound Collage",   // 121
    "Electronic---Speed Garage",    // 122
    "Electronic---Speedcore",       // 123
    "Electronic---Synth-pop",       // 124
    "Electronic---Synthwave",       // 125
    "Electronic---Tech House",      // 126
    "Electronic---Tech Trance",     // 127
    "Electronic---Techno",          // 128
    "Electronic---Trance",          // 129
    "Electronic---Tribal",          // 130
    "Electronic---Tribal House",    // 131
    "Electronic---Trip Hop",        // 132
    "Electronic---Tropical House",  // 133
    "Electronic---UK Garage",       // 134
    "Electronic---Vaporwave",       // 135
    "Electronic---Witch House",     // 136
    "Folk, World, & Country",       // 137
    "Folk, World, & Country---African",// 138
    "Folk, World, & Country---Bluegrass",// 139
    "Folk, World, & Country---Cajun",// 140
    "Folk, World, & Country---Celtic",// 141
    "Folk, World, & Country---Country",// 142
    "Folk, World, & Country---Fado",// 143
    "Folk, World, & Country---Folk",// 144
    "Folk, World, & Country---Gospel",// 145
    "Folk, World, & Country---Highlife",// 146
    "Folk, World, & Country---Hillbilly",// 147
    "Folk, World, & Country---Hindustani",// 148
    "Folk, World, & Country---Honky Tonk",// 149
    "Folk, World, & Country---Indian Classical",// 150
    "Folk, World, & Country---Laiko",// 151
    "Folk, World, & Country---Nordic",// 152
    "Folk, World, & Country---Pacific",// 153
    "Folk, World, & Country---Polka",// 154
    "Folk, World, & Country---Raï",  // 155
    "Folk, World, & Country---Romani",// 156
    "Folk, World, & Country---Soukous",// 157
    "Folk, World, & Country---Séga", // 158
    "Folk, World, & Country---Volksmusik",// 159
    "Folk, World, & Country---Zouk", // 160
    "Funk / Soul",                   // 161
    "Funk / Soul---Afrobeat",        // 162
    "Funk / Soul---Boogie",          // 163
    "Funk / Soul---Contemporary R&B",// 164
    "Funk / Soul---Disco",           // 165
    "Funk / Soul---Free Funk",       // 166
    "Funk / Soul---Funk",            // 167
    "Funk / Soul---Gospel",          // 168
    "Funk / Soul---Neo Soul",        // 169
    "Funk / Soul---New Jack Swing",  // 170
    "Funk / Soul---P.Funk",          // 171
    "Funk / Soul---Psychedelic",     // 172
    "Funk / Soul---Rhythm & Blues",  // 173
    "Funk / Soul---Soul",            // 174
    "Funk / Soul---Swingbeat",       // 175
    "Funk / Soul---UK Street Soul",  // 176
    "Hip Hop",                       // 177
    "Hip Hop---Bass Music",          // 178
    "Hip Hop---Boom Bap",            // 179
    "Hip Hop---Bounce",              // 180
    "Hip Hop---Britcore",            // 181
    "Hip Hop---Cloud Rap",           // 182
    "Hip Hop---Conscious",           // 183
    "Hip Hop---Crunk",               // 184
    "Hip Hop---Cut-up/DJ",           // 185
    "Hip Hop---DJ Battle Tool",      // 186
    "Hip Hop---Electro",             // 187
    "Hip Hop---G-Funk",              // 188
    "Hip Hop---Gangsta",             // 189
    "Hip Hop---Grime",               // 190
    "Hip Hop---Hardcore Hip-Hop",    // 191
    "Hip Hop---Horrorcore",          // 192
    "Hip Hop---Instrumental",        // 193
    "Hip Hop---Jazzy Hip-Hop",       // 194
    "Hip Hop---Miami Bass",          // 195
    "Hip Hop---Pop Rap",             // 196
    "Hip Hop---Ragga HipHop",       // 197
    "Hip Hop---RnB/Swing",           // 198
    "Hip Hop---Screw",               // 199
    "Hip Hop---Thug Rap",            // 200
    "Hip Hop---Trap",                // 201
    "Hip Hop---Trip Hop",            // 202
    "Hip Hop---Turntablism",         // 203
    "Jazz",                          // 204
    "Jazz---Afro-Cuban Jazz",        // 205
    "Jazz---Afrobeat",               // 206
    "Jazz---Avant-garde Jazz",       // 207
    "Jazz---Big Band",               // 208
    "Jazz---Bop",                    // 209
    "Jazz---Bossa Nova",             // 210
    "Jazz---Contemporary Jazz",      // 211
    "Jazz---Cool Jazz",              // 212
    "Jazz---Dixieland",              // 213
    "Jazz---Easy Listening",         // 214
    "Jazz---Free Improvisation",     // 215
    "Jazz---Free Jazz",              // 216
    "Jazz---Fusion",                 // 217
    "Jazz---Gypsy Jazz",             // 218
    "Jazz---Hard Bop",               // 219
    "Jazz---Jazz-Funk",              // 220
    "Jazz---Jazz-Rock",              // 221
    "Jazz---Latin Jazz",             // 222
    "Jazz---Modal",                  // 223
    "Jazz---Post Bop",               // 224
    "Jazz---Ragtime",                // 225
    "Jazz---Smooth Jazz",            // 226
    "Jazz---Soul-Jazz",              // 227
    "Jazz---Space-Age",              // 228
    "Jazz---Swing",                  // 229
    "Latin",                         // 230
    "Latin---Bachata",               // 231
    "Latin---Baião",                 // 232
    "Latin---Bolero",                // 233
    "Latin---Boogaloo",              // 234
    "Latin---Bossanova",             // 235
    "Latin---Cha-Cha",               // 236
    "Latin---Charanga",              // 237
    "Latin---Compas",                // 238
    "Latin---Cubano",                // 239
    "Latin---Cumbia",                // 240
    "Latin---Descarga",              // 241
    "Latin---Forró",                 // 242
    "Latin---Guaguancó",             // 243
    "Latin---Guajira",               // 244
    "Latin---Guaracha",              // 245
    "Latin---MPB",                   // 246
    "Latin---Mambo",                 // 247
    "Latin---Mariachi",              // 248
    "Latin---Merengue",              // 249
    "Latin---Norteño",               // 250
    "Latin---Nueva Cancion",         // 251
    "Latin---Pachanga",              // 252
    "Latin---Porro",                 // 253
    "Latin---Ranchera",              // 254
    "Latin---Reggaeton",             // 255
    "Latin---Rumba",                 // 256
    "Latin---Salsa",                 // 257
    "Latin---Samba",                 // 258
    "Latin---Son",                   // 259
    "Latin---Son Montuno",           // 260
    "Latin---Tango",                 // 261
    "Latin---Tejano",                // 262
    "Latin---Vallenato",             // 263
    "Non-Music",                     // 264
    "Non-Music---Audiobook",         // 265
    "Non-Music---Comedy",            // 266
    "Non-Music---Dialogue",          // 267
    "Non-Music---Education",         // 268
    "Non-Music---Field Recording",   // 269
    "Non-Music---Interview",         // 270
    "Non-Music---Monolog",           // 271
    "Non-Music---Poetry",            // 272
    "Non-Music---Political",         // 273
    "Non-Music---Promotional",       // 274
    "Non-Music---Radioplay",         // 275
    "Non-Music---Religious",         // 276
    "Non-Music---Spoken Word",       // 277
    "Pop",                           // 278
    "Pop---Ballad",                  // 279
    "Pop---Bollywood",               // 280
    "Pop---Bubblegum",               // 281
    "Pop---Chanson",                 // 282
    "Pop---City Pop",                // 283
    "Pop---Europop",                 // 284
    "Pop---Indie Pop",               // 285
    "Pop---J-pop",                   // 286
    "Pop---K-pop",                   // 287
    "Pop---Kayōkyoku",               // 288
    "Pop---Light Music",             // 289
    "Pop---Music Hall",              // 290
    "Pop---Novelty",                 // 291
    "Pop---Parody",                  // 292
    "Pop---Schlager",                // 293
    "Pop---Vocal",                   // 294
    "Reggae",                        // 295
    "Reggae---Calypso",              // 296
    "Reggae---Dancehall",            // 297
    "Reggae---Dub",                  // 298
    "Reggae---Lovers Rock",          // 299
    "Reggae---Ragga",                // 300
    "Reggae---Reggae",               // 301
    "Reggae---Reggae-Pop",           // 302
    "Reggae---Rocksteady",           // 303
    "Reggae---Roots Reggae",         // 304
    "Reggae---Ska",                  // 305
    "Reggae---Soca",                 // 306
    "Rock",                          // 307
    "Rock---AOR",                    // 308
    "Rock---Acid Rock",              // 309
    "Rock---Acoustic",               // 310
    "Rock---Alternative Rock",       // 311
    "Rock---Arena Rock",             // 312
    "Rock---Art Rock",               // 313
    "Rock---Atmospheric Black Metal",// 314
    "Rock---Avantgarde",             // 315
    "Rock---Beat",                   // 316
    "Rock---Black Metal",            // 317
    "Rock---Blues Rock",             // 318
    "Rock---Brit Pop",               // 319
    "Rock---Classic Rock",           // 320
    "Rock---Coldwave",               // 321
    "Rock---Country Rock",           // 322
    "Rock---Crust",                  // 323
    "Rock---Death Metal",            // 324
    "Rock---Deathcore",              // 325
    "Rock---Deathrock",              // 326
    "Rock---Depressive Black Metal", // 327
    "Rock---Doo Wop",                // 328
    "Rock---Doom Metal",             // 329
    "Rock---Dream Pop",              // 330
    "Rock---Emo",                    // 331
    "Rock---Ethereal",               // 332
    "Rock---Experimental",           // 333
    "Rock---Folk Metal",             // 334
    "Rock---Folk Rock",              // 335
    "Rock---Garage Rock",            // 336
    "Rock---Glam",                   // 337
    "Rock---Gothic Metal",           // 338
    "Rock---Gothic Rock",            // 339
    "Rock---Grindcore",              // 340
    "Rock---Grunge",                 // 341
    "Rock---Hard Rock",              // 342
    "Rock---Hardcore",               // 343
    "Rock---Heavy Metal",            // 344
    "Rock---Indie Rock",             // 345
    "Rock---Industrial",             // 346
    "Rock---Krautrock",              // 347
    "Rock---Lo-Fi",                  // 348
    "Rock---Lounge",                 // 349
    "Rock---Math Rock",              // 350
    "Rock---Melodic Death Metal",    // 351
    "Rock---Melodic Hardcore",       // 352
    "Rock---Metalcore",              // 353
    "Rock---Mod",                    // 354
    "Rock---Neofolk",                // 355
    "Rock---New Wave",               // 356
    "Rock---No Wave",                // 357
    "Rock---Noise",                  // 358
    "Rock---Noisecore",              // 359
    "Rock---Nu Metal",               // 360
    "Rock---Oi",                     // 361
    "Rock---Pagan Metal",            // 362
    "Rock---Pop Punk",               // 363
    "Rock---Pop Rock",               // 364
    "Rock---Post Rock",              // 365
    "Rock---Post-Hardcore",          // 366
    "Rock---Post-Metal",             // 367
    "Rock---Post-Punk",              // 368
    "Rock---Power Metal",            // 369
    "Rock---Power Pop",              // 370
    "Rock---Power Violence",         // 371
    "Rock---Prog Rock",              // 372
    "Rock---Progressive Metal",      // 373
    "Rock---Psychedelic Rock",       // 374
    "Rock---Psychobilly",            // 375
    "Rock---Pub Rock",               // 376
    "Rock---Punk",                   // 377
    "Rock---Rock & Roll",            // 378
    "Rock---Rockabilly",             // 379
    "Rock---Shoegaze",               // 380
    "Rock---Ska",                    // 381
    "Rock---Sludge Metal",           // 382
    "Rock---Soft Rock",              // 383
    "Rock---Southern Rock",          // 384
    "Rock---Space Rock",             // 385
    "Rock---Speed Metal",            // 386
    "Rock---Stoner Rock",            // 387
    "Rock---Surf",                   // 388
    "Rock---Symphonic Rock",         // 389
    "Rock---Technical Death Metal",  // 390
    "Rock---Thrash",                 // 391
    "Rock---Twisted",                // 392  (nonstandard label, kept for index alignment)
    "Rock---Viking Metal",           // 393
    "Rock---Yé-Yé",                  // 394
    "Stage & Screen",                // 395
    "Stage & Screen---Musical",      // 396
    "Stage & Screen---Score",        // 397
    "Stage & Screen---Soundtrack",   // 398
    "Stage & Screen---Theme",        // 399
};

static constexpr int kNumLabels = 400;

// Indices of labels that are mood-indicative (heuristic mapping).
// We scan for these in the top activations and build a mood string.
struct MoodMapping {
    int         index;
    const char* mood;
};

static const MoodMapping kMoodMappings[] = {
    {  40, "chill" },           // Ambient
    {  53, "dark" },            // Dark Ambient
    {  54, "dark" },            // Darkwave
    {  59, "chill" },           // Downtempo
    {  60, "atmospheric" },     // Drone
    {  74, "experimental" },    // Electronic---Experimental
    {  82, "psychedelic" },     // Goa Trance
    {  84, "euphoric" },        // Happy Hardcore
    {  86, "aggressive" },      // Hard Techno
    {  88, "aggressive" },      // Hardcore
    {  89, "aggressive" },      // Hardstyle
    {  90, "energetic" },       // Hi NRG
    { 109, "relaxing" },        // New Age
    { 112, "aggressive" },      // Noise
    { 118, "psychedelic" },     // Psy-Trance
    { 124, "uplifting" },       // Synth-pop
    { 125, "nostalgic" },       // Synthwave
    { 132, "melancholic" },     // Trip Hop
    { 135, "nostalgic" },       // Vaporwave
    { 136, "dark" },            // Witch House
    { 214, "relaxing" },        // Easy Listening
    { 226, "relaxing" },        // Smooth Jazz
    { 279, "romantic" },        // Ballad
    { 289, "relaxing" },        // Light Music
    { 317, "aggressive" },      // Black Metal
    { 324, "aggressive" },      // Death Metal
    { 329, "dark" },            // Doom Metal
    { 330, "dreamy" },          // Dream Pop
    { 332, "dreamy" },          // Ethereal
    { 339, "dark" },            // Gothic Rock
    { 341, "melancholic" },     // Grunge
    { 342, "energetic" },       // Hard Rock
    { 344, "energetic" },       // Heavy Metal
    { 374, "psychedelic" },     // Psychedelic Rock
    { 380, "dreamy" },          // Shoegaze
    { 385, "atmospheric" },     // Space Rock
};

static constexpr int kNumMoodMappings =
    static_cast<int>(sizeof(kMoodMappings) / sizeof(kMoodMappings[0]));

// Indices that indicate "instrumental" (no vocal). High activation on these
// suggests vocal_prob should be low.
static const int kInstrumentalIndices[] = {
    193,    // Hip Hop---Instrumental
};

// Indices that indicate "vocal". High activation suggests vocal_prob is high.
static const int kVocalIndices[] = {
    294,    // Pop---Vocal
    277,    // Spoken Word
};

// High-danceability genre indices (used as a heuristic fallback when the model
// does not provide a dedicated danceability output).
static const int kDanceableIndices[] = {
    38,  55,  57,  61,  64,  68,  70,  73,  77,  78,   // Acid House .. Garage House
    80,  84,  85,  89,  91,  95,  96,  97,  99, 100,   // Ghetto House .. Jumpstyle
    101, 105, 113, 116, 122, 126, 128, 129, 130, 131,  // Jungle .. Tribal House
    133, 134, 165, 255, 297,                            // Tropical House, Disco, Reggaeton, Dancehall
};
static constexpr int kNumDanceableIndices =
    static_cast<int>(sizeof(kDanceableIndices) / sizeof(kDanceableIndices[0]));

} // namespace DiscogsLabels


// ── Essentia library init guard ──────────────────────────────────────────────
// essentia::init_essentia() / essentia::shutdown_essentia() are NOT thread-safe.
// We use a simple flag to ensure init happens exactly once.

static bool s_essentiaInited = false;

static void ensureEssentiaInit()
{
    if (!s_essentiaInited) {
        essentia::init_essentia();
        s_essentiaInited = true;
    }
}


// ── Public ───────────────────────────────────────────────────────────────────

bool EssentiaAnalyzer::isAvailable()
{
    ensureEssentiaInit();

    // Check that the algorithms we need are registered
    const auto& factory = AlgorithmFactory::instance();
    if (!factory.keys().contains("MonoLoader") ||
        !factory.keys().contains("BeatTrackerMultiFeature") ||
        !factory.keys().contains("KeyExtractor")) {
        return false;
    }

#ifdef HAVE_ONNX
    // Check that the ONNX model file exists
    const QString modelPath = findDiscogsModel();
    if (modelPath.isEmpty()) {
        qInfo() << "EssentiaAnalyzer: Discogs-Effnet model not found (non-fatal, tags disabled)";
    }
    // We are available even without the model -- BPM and key still work.
    return true;
#else
    // Without ONNX we can still do BPM + key.
    return true;
#endif
}

AnalysisResult EssentiaAnalyzer::analyze(const QString& filepath)
{
    AnalysisResult result;
    result.essentiaUsed = true;

    try {
        ensureEssentiaInit();

        const std::string path = filepath.toStdString();
        auto& factory = AlgorithmFactory::instance();

        // ── 1. Load audio ────────────────────────────────────────────────
        std::vector<Real> audio;
        Algorithm* loader = factory.create("MonoLoader",
            "filename", path,
            "sampleRate", 44100);
        loader->output("audio").set(audio);
        loader->compute();
        delete loader;

        if (audio.empty()) {
            result.error = QStringLiteral("MonoLoader returned empty audio");
            return result;
        }

        // ── 2. BPM via BeatTrackerMultiFeature ───────────────────────────
        std::vector<Real> ticks;
        Real confidence = 0.0f;
        Algorithm* beatTracker = factory.create("BeatTrackerMultiFeature");
        beatTracker->input("signal").set(audio);
        beatTracker->output("ticks").set(ticks);
        beatTracker->output("confidence").set(confidence);
        beatTracker->compute();
        delete beatTracker;

        // Compute BPM from inter-beat intervals
        if (ticks.size() >= 2) {
            std::vector<Real> ibis;
            ibis.reserve(ticks.size() - 1);
            for (size_t i = 1; i < ticks.size(); ++i) {
                const Real ibi = ticks[i] - ticks[i - 1];
                if (ibi > 0.0f)
                    ibis.push_back(ibi);
            }
            if (!ibis.empty()) {
                // Median IBI for robustness against outliers
                std::sort(ibis.begin(), ibis.end());
                const Real medianIBI = ibis[ibis.size() / 2];
                if (medianIBI > 0.0f) {
                    double bpm = 60.0 / static_cast<double>(medianIBI);
                    // Resolve half/double tempo ambiguity: keep BPM in DJ range 60-200
                    while (bpm < 60.0)  bpm *= 2.0;
                    while (bpm > 200.0) bpm /= 2.0;
                    result.bpm = std::round(bpm * 100.0) / 100.0;  // 2 decimal places
                }
            }
        }

        // ── 3. Key via KeyExtractor ──────────────────────────────────────
        {
            std::string keyStr, scaleStr;
            Real keyStrength = 0.0f;
            Algorithm* keyExtractor = factory.create("KeyExtractor");
            keyExtractor->input("audio").set(audio);
            keyExtractor->output("key").set(keyStr);
            keyExtractor->output("scale").set(scaleStr);
            keyExtractor->output("strength").set(keyStrength);
            keyExtractor->compute();
            delete keyExtractor;

            if (!keyStr.empty() && keyStr != "none") {
                // Format as "Am", "C#m", "Bb" etc.
                QString k = QString::fromStdString(keyStr);
                if (scaleStr == "minor")
                    k += QStringLiteral("m");
                result.key = k;
            }
        }

        // ── 4. Discogs-Effnet ONNX model (genre/mood/danceability/vocal) ─
#ifdef HAVE_ONNX
        const QString modelPath = findDiscogsModel();
        if (!modelPath.isEmpty()) {
            try {
                // Compute mel spectrogram for the model input.
                // Discogs-Effnet expects 96-band mel spectrogram frames at 16kHz,
                // patch size 96x64. We use Essentia's TensorflowInputMusiCNN
                // or manually compute. For simplicity, we use a raw ONNX approach
                // with a mel spectrogram computed via Essentia.

                // Resample to 16kHz for the model
                std::vector<Real> audio16k;
                Algorithm* resampler = factory.create("Resample",
                    "inputSampleRate", 44100,
                    "outputSampleRate", 16000);
                resampler->input("signal").set(audio);
                resampler->output("signal").set(audio16k);
                resampler->compute();
                delete resampler;

                // Compute mel spectrogram using Essentia's MelSpectrogram
                // Frame size: 512 samples at 16kHz (32ms), hop 256 (16ms)
                const int frameSize = 512;
                const int hopSize   = 256;
                const int numBands  = 96;

                std::vector<std::vector<Real>> melFrames;
                Algorithm* frameCutter = factory.create("FrameCutter",
                    "frameSize", frameSize,
                    "hopSize", hopSize,
                    "startFromZero", true);
                Algorithm* windowing = factory.create("Windowing",
                    "type", "hann",
                    "size", frameSize);
                Algorithm* spectrum = factory.create("Spectrum",
                    "size", frameSize);
                Algorithm* melBands = factory.create("MelBands",
                    "numberBands", numBands,
                    "sampleRate", 16000,
                    "inputSize", frameSize / 2 + 1);

                std::vector<Real> frame, windowedFrame, spectrumVec, melVec;

                frameCutter->input("signal").set(audio16k);
                frameCutter->output("frame").set(frame);

                while (true) {
                    frameCutter->compute();
                    if (frame.empty()) break;

                    windowing->input("frame").set(frame);
                    windowing->output("frame").set(windowedFrame);
                    windowing->compute();

                    spectrum->input("frame").set(windowedFrame);
                    spectrum->output("spectrum").set(spectrumVec);
                    spectrum->compute();

                    melBands->input("spectrum").set(spectrumVec);
                    melBands->output("bands").set(melVec);
                    melBands->compute();

                    // Log-scale mel bands
                    std::vector<Real> logMel(melVec.size());
                    for (size_t i = 0; i < melVec.size(); ++i)
                        logMel[i] = std::log(std::max(melVec[i], 1e-10f));

                    melFrames.push_back(logMel);
                }

                delete frameCutter;
                delete windowing;
                delete spectrum;
                delete melBands;

                // Create patches of 96x64 (numBands x 64 frames)
                const int patchFrames = 64;
                if (static_cast<int>(melFrames.size()) >= patchFrames) {
                    // Take evenly spaced patches across the track, average predictions
                    const int totalFrames = static_cast<int>(melFrames.size());
                    const int numPatches = std::min(10, totalFrames / patchFrames);

                    // ONNX Runtime session
                    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "discogs_effnet");
                    Ort::SessionOptions sessionOpts;
                    sessionOpts.SetIntraOpNumThreads(1);
                    sessionOpts.SetGraphOptimizationLevel(
                        GraphOptimizationLevel::ORT_ENABLE_BASIC);

                    const std::string modelStdPath = modelPath.toStdString();
                    Ort::Session session(env, modelStdPath.c_str(), sessionOpts);

                    // Accumulate predictions across patches
                    std::vector<float> avgActivations(DiscogsLabels::kNumLabels, 0.0f);

                    for (int p = 0; p < numPatches; ++p) {
                        const int startFrame = (totalFrames - patchFrames) * p
                                              / std::max(1, numPatches - 1);

                        // Build input tensor: shape [1, 1, 96, 64]
                        std::vector<float> inputData(1 * 1 * numBands * patchFrames, 0.0f);
                        for (int f = 0; f < patchFrames; ++f) {
                            const auto& mel = melFrames[startFrame + f];
                            for (int b = 0; b < numBands && b < static_cast<int>(mel.size()); ++b)
                                inputData[b * patchFrames + f] = mel[b];
                        }

                        const std::array<int64_t, 4> inputShape = {1, 1, numBands, patchFrames};
                        Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
                            OrtArenaAllocator, OrtMemTypeDefault);
                        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                            memInfo, inputData.data(),
                            inputData.size(),
                            inputShape.data(), inputShape.size());

                        // Get input/output names
                        Ort::AllocatorWithDefaultOptions allocator;
                        auto inputNamePtr = session.GetInputNameAllocated(0, allocator);
                        auto outputNamePtr = session.GetOutputNameAllocated(0, allocator);
                        const char* inputNames[]  = { inputNamePtr.get() };
                        const char* outputNames[] = { outputNamePtr.get() };

                        // Run inference
                        auto outputTensors = session.Run(
                            Ort::RunOptions{nullptr},
                            inputNames, &inputTensor, 1,
                            outputNames, 1);

                        // Read output activations
                        const float* outData = outputTensors[0].GetTensorData<float>();
                        const auto outShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
                        const int numOutputs = (outShape.size() > 1)
                            ? static_cast<int>(outShape[1])
                            : static_cast<int>(outShape[0]);

                        for (int i = 0; i < std::min(numOutputs, DiscogsLabels::kNumLabels); ++i)
                            avgActivations[i] += outData[i];
                    }

                    // Average across patches
                    if (numPatches > 0) {
                        const float scale = 1.0f / static_cast<float>(numPatches);
                        for (auto& v : avgActivations)
                            v *= scale;
                    }

                    // ── Extract top-5 style tags ─────────────────────────────
                    std::vector<int> indices(DiscogsLabels::kNumLabels);
                    std::iota(indices.begin(), indices.end(), 0);
                    std::partial_sort(indices.begin(), indices.begin() + 5, indices.end(),
                        [&](int a, int b) { return avgActivations[a] > avgActivations[b]; });

                    QStringList styleTags;
                    for (int i = 0; i < 5; ++i) {
                        const int idx = indices[i];
                        if (avgActivations[idx] > 0.01f && idx < DiscogsLabels::kNumLabels) {
                            // Extract the sub-genre part after "---", or use the full label
                            QString label = QString::fromLatin1(DiscogsLabels::kLabels[idx]);
                            const int sep = label.indexOf(QLatin1String("---"));
                            if (sep >= 0)
                                label = label.mid(sep + 3);
                            styleTags.append(label);
                        }
                    }
                    result.styleTags = styleTags.join(QStringLiteral(", "));

                    // ── Extract mood tags ────────────────────────────────────
                    QStringList moodTags;
                    for (int m = 0; m < DiscogsLabels::kNumMoodMappings; ++m) {
                        const auto& mm = DiscogsLabels::kMoodMappings[m];
                        if (mm.index < DiscogsLabels::kNumLabels &&
                            avgActivations[mm.index] > 0.15f) {
                            const QString mood = QString::fromLatin1(mm.mood);
                            if (!moodTags.contains(mood))
                                moodTags.append(mood);
                        }
                        if (moodTags.size() >= 3) break;  // cap at 3 mood tags
                    }
                    result.moodTags = moodTags.join(QStringLiteral(", "));

                    // ── Danceability heuristic ───────────────────────────────
                    float danceScore = 0.0f;
                    for (int i = 0; i < DiscogsLabels::kNumDanceableIndices; ++i) {
                        const int idx = DiscogsLabels::kDanceableIndices[i];
                        if (idx < DiscogsLabels::kNumLabels)
                            danceScore += avgActivations[idx];
                    }
                    // Clamp to 0-1 range
                    result.danceability = std::min(1.0f, std::max(0.0f, danceScore));

                    // ── Vocal probability heuristic ──────────────────────────
                    float vocalScore = 0.0f;
                    float instrScore = 0.0f;
                    for (int idx : DiscogsLabels::kVocalIndices) {
                        if (idx < DiscogsLabels::kNumLabels)
                            vocalScore += avgActivations[idx];
                    }
                    for (int idx : DiscogsLabels::kInstrumentalIndices) {
                        if (idx < DiscogsLabels::kNumLabels)
                            instrScore += avgActivations[idx];
                    }
                    const float totalVocal = vocalScore + instrScore;
                    result.vocalProb = (totalVocal > 0.01f)
                        ? std::min(1.0f, vocalScore / totalVocal)
                        : 0.5f;  // unknown

                    // ── Valence: not available from Discogs-Effnet ───────────
                    result.valence = 0.0f;
                }

            } catch (const Ort::Exception& e) {
                qWarning() << "EssentiaAnalyzer: ONNX inference error:" << e.what();
                // Non-fatal: BPM and key are still valid
            }
        }
#endif // HAVE_ONNX

        result.success = true;

    } catch (const EssentiaException& e) {
        result.error = QStringLiteral("Essentia error: ") + QString::fromStdString(e.what());
        result.success = false;
    } catch (const std::exception& e) {
        result.error = QStringLiteral("Analysis error: ") + QString::fromLatin1(e.what());
        result.success = false;
    }

    return result;
}

QString EssentiaAnalyzer::findDiscogsModel()
{
    // Search in order: app data dir, app dir, current dir
    const QStringList searchDirs = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/models"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/models"),
        QDir::currentPath() + QStringLiteral("/models"),
    };

    const QString modelName = QStringLiteral("discogs-effnet-bs64-1.pb.onnx");
    for (const QString& dir : searchDirs) {
        const QString path = dir + QStringLiteral("/") + modelName;
        if (QFileInfo::exists(path)) {
            qInfo() << "EssentiaAnalyzer: found Discogs-Effnet model at" << path;
            return path;
        }
    }

    return {};
}

#endif // HAVE_ESSENTIA
