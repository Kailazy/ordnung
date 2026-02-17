"""One-time seed: import 'minimal world' and apply FLAC selections."""
import os
from db import init_db, get_connection
from importer import parse_rekordbox_txt

TXT_PATH = r"C:\Users\kaila\OneDrive\Desktop\minimal world.txt"
PLAYLIST_NAME = "minimal world"

FLAC_EXPORT = """Copacabannark - Cave (Original Mix)
Rhadoo, Colorhadoo - Zanzibar (Original Mix)
TM Shuffle - Isolation (Original Mix)
Mandar -Antithetical Affirmation - Oscillat 03 www.myfreemp3.biz
Monolake - Arte
Petre Inspirescu - Basso Ostinato (Original Mix)
Monolake - Arte
Laurine Frost - Let's Feed The Wolf (Petre Inspirescu Remix)
Andy Stott - Long Drive
Neneh Cherry - Everything (Vilod High Blood Pressure Mix)
tINI - Mine Has A Shower
tINI - Blond Galipette
Rod Modell - Space Age Mythology
Andy Stott - Made Your Point
lb honne - pa ohm
Barac - C1 Cantarea Celor 3 Tineri VINYL ONLY
Young Seth - Moment (Original Mix)
Mari Kvien Brunvoll & Ricardo Villalobos - Everywhere You Go Villalobos celestial voice resurrectionamnesia rehabilitation mix
Ultrakurt - Candy Raton
Move D - Like I Was King  (Black Label Mix)
Cabanne - Double Lardon
Pretending - Erik Luebs (Original Mix) 134
Delaj - 1A - B1 Take My Time VINYL ONLY
Roon - Select (Malin Genie Remix) Vinyl Only
Vince Watson - My Subconscious
[UVARBLK001] Swoy - Daydreamer
Koreless - Deceltica
Loxodrome - Drop Out
Atjazz - For Real (Version Remix)
Dana Ruh - Reach Out For Me
Donato Dozzy - Cassandra
Archie Hamilton & Benson Herbert - Swerve (Original Mix)
Laughing Man & Viktor Udvari - Shades
Pole - Pirol
Rhadoo - Circul Globus
Rhadoo - Slagare
Claro Intelecto - Momento
Rhadoo - Slagare
Margaret Dygas - Popular Religions
Claro Intelecto - Thieves
Octo Octa - Adrift (Dorisburg Remix)
Beta Librae - Urras
tilman ehrhorn - pass
Sistol - Keno
Sistol - Kojo
Vladislav Delay - Huone
Abi, Tommy Vercetti - I Want You (Original Mix)
Margaret Dygas - No Now
Loidis - 01 Tell Me
Buttechno - Silent E
Priori - Thick Air
Priori - Silicate Tusks
Gramm - Legends / Nugroove TM
Gramm - Ment
CiM - Soft Rain
Ricardo Villalobos - Spritzcussion [RV GM Sample Mix]
Mighty Dub Katz - Let The Drums Speak (Butch Remix)
Heiko Laux & Johannes Heil \u200e- No Gain No Pain ( Ricardo Villalobos Mix )
Matias Aguayo x Ricardo Villalobos - El Camar\u00f3n (Ricardo Villalobos Remix)
DJ Gregory - 9A - Head Talking
David Squillace & Swat-Squad - Panik Reinterpretation
Zenk - Juicy Breakfast
Zenk - Nairobi Market
Zenk - Nairobi Market (Ted Amber Remix)
Move D - Like I Was King (Black Label Mix)
Ultrakurt - Candy Raton
The Persuader - Sun Position (Cabanne Remix)
Cabanne, Varhat - Chhulub (Cabanne Remix)
Swoy - Daydreamer
Lessi S. & Paolo Rocco  - Feel That Way (Pij
Neu Balance - Restate - Dance Edit
Chevel - Viewpoint
Margaret Dygas - For Five
Ion Ludwig - Cold Fog and Smoke (Ahead)
Ion Ludwig - The Stabbed Space
Ion Ludwig - Conciliation
SCB - Down Moment
Margaret Dygas - Quintet
Mihai Pol - Mounting
Voiski, WAV & Wata Igarashi - Pronom [Delsin Records]
Ricardo Villalobos - Humusweg
Aparte & Dan Blatov - Guidance
Achterbahn d'amour - trance me up [i wanna go higher] (skudge remix)
Savvas Ysatis - Mar's Bar
Auguste Safar & Raphael Graham - Za Ria (Ric Y Martin Remix)
Wax - 50005A (Original Mix)
Ricardo Villalobos - Tempura
Cab Drivers - Cab Drivers - Playroom
Ricardo Villalobos - Suesse Cheques (Original Mix)
Dana Ruh - Reach Out For Me (Original Mix)
B\u00e4rtaub - Ya Betta Jam
Will Hofbauer - Rocks Off (Original Mix)
Bas Dobbelaer, Vand - Nomansland (Original Mix)
Donato Dozzy - Cassandra (Original Mix)
Martin Buttrich - Tripping In The 16th
Adam Feingold - Spiral Kiss, Labyrinth Mist
Adam Feingold - Cusp of Spring
Adam Feingold - A Frame for U
Adam Feingold - Ten yr Loop
Model 500 - Starlight
Loxodrome - Drop Out
Answer Code Request - Thermal Capacity
Alex Celler - Pacificon
Margaret Dygas - His Name Is Ken
Farben - Balladen (Beweglich)
Margaret Dygas - Day After
Eduardo De La Calle - Supreme Blue
Margaret Dygas - No Now
Erik Luebs - Transform Into Glass
Ryan Elliott - Abatis
Dorisburg - Gripen
Robert Hood - Escapes
Richie Hawtin - Richie Hawtin "Concept 1 96:07 14:00"
Endian - Two Chords Deep
Antonio Thagma - Sferikum (Ricardo Villalobos Remix)
Adrian Niculae - conTRASt
Exos - Rigning \u00ed Stekknum
Sit - Jazzocorason
Terranova, Dettinger, Reinhard Voigt, Studio 1 - Take My Hand  Oasis 4  Rosa 1  B\u00fcrger Sichten Falschgeld (mixed) - www.djsoundtop.com
Vlad Caia - Projections (Fumiya Tanaka Remix)
Vlad Caia - Projections (Original Mix)
Turner - When Will We Leave (Robert Hood Remix)
AEM Rhythm-Cascade - New Day
Theo Parrish - Synthetic flemm
Dense & Pika - Colt (Extended Mix)
Ben Klock ft. Elif Bicer - Goodly Sin (Robert Hood rmx)
DJ Trystero - Oriel
Atomic Moog - Blue Scale
DJ Trystero - Gutter
Basic Channel - Radiance II
Richie Hawtin - Richie Hawtin "Concept 1 96:01 01:00"
Kosh - Whiplash
Baby Ford - All That Nothing
Seuil - Oamer (Baby Ford Remix)
Baby Ford & Eon - Dead Eye
Richie Hawtin - Richie Hawtin "Concept 1 96:02 04:00"
Shinichi Atobe - Free Access Zone 7
Dragosh - Have A
Mike Dehnert - Montage
Maurizio - Domina (Maurizio Mix) (Edit)
Substance - Sensualize
Baby Ford - Dead Eye
Baby Ford - Plaza
Baby Ford - Monolense
Main Street - Round Four Feat. Paul St. Hilaire - Found A Way
Traumprinz - I Love Ya
Prince of Denmark - Squidcall
Traumprinz - Hey Baby
Traumprinz - Freedom
Prince of Denmark - Cut 06
Studio 1 - Rot 1
Traumprinz - Where is Home?
DJ Skull - Get Em
Peverelist - Pulse XV (Original Mix)
Peverelist - Pulse XIX
Losian - He's My Brother
Maus & Stolle - Pan
Maus & Stolle - Sparks
The Vegetable Orchestra - Ciboulette (Luciano Remix)
Studio 1 - Rot 3
Studio 1 - Rot 2
Studio 1 - Grun 3
Studio 1 - Grun 2
Studio 1 - Grun 1
Derek Carr - Destiny
Dettinger - Blond 1
Dettinger - Blond 3
INTe*ra - untitled
INTe*ra - 410
INTe*ra - 414
PLO Man - fig. 003
INTe*ra - 548
PLO Man - fig. 002
INTe*ra - 697
PLO Man - fig. 015
PLO Man - fig. 001
SnPLO - D
SnPLO - C (ft. roadkill)
SnPLO - B
SnPLO - A
KL MAUSSTOLLETYPEBEAT ZC MASTER V2
KL TEARDROPS ZC MASTER V1"""


def build_key(artist, title):
    """Build the same key format the export uses: 'artist - title' or just 'title'."""
    if artist:
        return f"{artist} - {title}"
    return title


def main():
    init_db()
    conn = get_connection()

    existing = conn.execute(
        "SELECT id FROM playlists WHERE name = ?", (PLAYLIST_NAME,)
    ).fetchone()
    if existing:
        print(f"Playlist '{PLAYLIST_NAME}' already exists (id={existing['id']}), skipping import.")
        print("Delete it from the UI if you want to re-import.")
        conn.close()
        return

    if not os.path.exists(TXT_PATH):
        print(f"Source file not found: {TXT_PATH}")
        conn.close()
        return

    tracks = parse_rekordbox_txt(TXT_PATH)
    print(f"Parsed {len(tracks)} tracks from txt")

    cur = conn.execute("INSERT INTO playlists (name) VALUES (?)", (PLAYLIST_NAME,))
    playlist_id = cur.lastrowid

    for t in tracks:
        conn.execute("""
            INSERT INTO tracks (playlist_id, title, artist, album, genre, bpm,
                                rating, time, key_sig, date_added)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (playlist_id, t["title"], t["artist"], t["album"], t["genre"],
              t["bpm"], t["rating"], t["time"], t["key"], t["date_added"]))

    conn.commit()

    # Build FLAC lookup set
    flac_lines = set()
    for line in FLAC_EXPORT.strip().splitlines():
        stripped = line.strip()
        if stripped:
            flac_lines.add(stripped)

    # Also normalize: collapse multiple spaces
    flac_normalized = set()
    for line in flac_lines:
        flac_normalized.add(" ".join(line.split()))

    # Match tracks and set to flac
    rows = conn.execute(
        "SELECT id, title, artist FROM tracks WHERE playlist_id = ?", (playlist_id,)
    ).fetchall()

    matched = 0
    for row in rows:
        key = build_key(row["artist"], row["title"])
        key_norm = " ".join(key.split())
        if key in flac_lines or key_norm in flac_normalized:
            conn.execute("UPDATE tracks SET format = 'flac' WHERE id = ?", (row["id"],))
            matched += 1

    conn.commit()
    conn.close()

    total = len(tracks)
    print(f"Imported '{PLAYLIST_NAME}': {total} tracks, {matched} marked as flac")


if __name__ == "__main__":
    main()
