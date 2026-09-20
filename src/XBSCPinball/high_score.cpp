#include "pch.h"
#include "high_score.h"

bool high_score::dlg_enter_name = false;
bool high_score::ShowDialog = false;
high_score_entry high_score::DlgData{};
std::vector<high_score_entry> high_score::ScoreQueue{};
high_score_struct high_score::highscore_table[5];

static const char* kHighScorePath = "D:\\highscore.dat";
static const unsigned char kHighScoreMagic[8] =
{
    'X', 'B', 'S', 'C', 'H', 'S', '0', '1'
};
static const unsigned kHighScoreVersion = 1;
static const unsigned kHighScoreCount = 5;

static bool ReadBytes(FILE* f, void* dst, size_t bytes)
{
    return f && dst && fread(dst, 1, bytes, f) == bytes;
}

static bool WriteBytes(FILE* f, const void* src, size_t bytes)
{
    return f && src && fwrite(src, 1, bytes, f) == bytes;
}

static bool ReadU32(FILE* f, unsigned& value)
{
    unsigned char b[4];
    if (!ReadBytes(f, b, sizeof(b)))
        return false;

    value =
        (unsigned)b[0] |
        ((unsigned)b[1] << 8) |
        ((unsigned)b[2] << 16) |
        ((unsigned)b[3] << 24);
    return true;
}

static bool WriteU32(FILE* f, unsigned value)
{
    unsigned char b[4];
    b[0] = (unsigned char)(value & 0xFF);
    b[1] = (unsigned char)((value >> 8) & 0xFF);
    b[2] = (unsigned char)((value >> 16) & 0xFF);
    b[3] = (unsigned char)((value >> 24) & 0xFF);
    return WriteBytes(f, b, sizeof(b));
}

static unsigned HashByte(unsigned hash, unsigned char value)
{
    // FNV-1a 32-bit. Simple, deterministic corruption check.
    hash ^= value;
    hash *= 16777619u;
    return hash;
}

static unsigned HighScoreChecksum()
{
    unsigned hash = 2166136261u;

    for (int i = 0; i < 5; ++i)
    {
        const high_score_struct& row = high_score::highscore_table[i];

        for (int j = 0; j < 32; ++j)
            hash = HashByte(hash, (unsigned char)row.Name[j]);

        const unsigned scoreBits = (unsigned)row.Score;
        hash = HashByte(hash, (unsigned char)(scoreBits & 0xFF));
        hash = HashByte(hash, (unsigned char)((scoreBits >> 8) & 0xFF));
        hash = HashByte(hash, (unsigned char)((scoreBits >> 16) & 0xFF));
        hash = HashByte(hash, (unsigned char)((scoreBits >> 24) & 0xFF));
    }

    return hash;
}

int high_score::read()
{
    clear_table();

    FILE* f = fopenu(kHighScorePath, "rb");
    if (!f)
        return 0; // First run is normal.

    unsigned char magic[8];
    unsigned version = 0;
    unsigned count = 0;

    if (!ReadBytes(f, magic, sizeof(magic)) ||
        memcmp(magic, kHighScoreMagic, sizeof(magic)) != 0 ||
        !ReadU32(f, version) ||
        !ReadU32(f, count) ||
        version != kHighScoreVersion ||
        count != kHighScoreCount)
    {
        fclose(f);
        clear_table();
        return 0;
    }

    for (int i = 0; i < 5; ++i)
    {
        unsigned scoreBits = 0;

        if (!ReadBytes(f, highscore_table[i].Name, sizeof(highscore_table[i].Name)) ||
            !ReadU32(f, scoreBits))
        {
            fclose(f);
            clear_table();
            return 0;
        }

        highscore_table[i].Name[31] = 0;
        highscore_table[i].Score = (int)scoreBits;
    }

    unsigned storedChecksum = 0;
    const bool checksumRead = ReadU32(f, storedChecksum);
    fclose(f);

    if (!checksumRead || storedChecksum != HighScoreChecksum())
        clear_table();

    return 0;
}

int high_score::write()
{
    FILE* f = fopenu(kHighScorePath, "wb");
    if (!f)
        return 1;

    bool ok =
        WriteBytes(f, kHighScoreMagic, sizeof(kHighScoreMagic)) &&
        WriteU32(f, kHighScoreVersion) &&
        WriteU32(f, kHighScoreCount);

    for (int i = 0; i < 5 && ok; ++i)
    {
        highscore_table[i].Name[31] = 0;

        ok =
            WriteBytes(f, highscore_table[i].Name, sizeof(highscore_table[i].Name)) &&
            WriteU32(f, (unsigned)highscore_table[i].Score);
    }

    if (ok)
        ok = WriteU32(f, HighScoreChecksum());

    fclose(f);
    return ok ? 0 : 1;
}

void high_score::clear_table()
{
    for (int i = 0; i < 5; ++i)
    {
        memset(highscore_table[i].Name, 0, sizeof(highscore_table[i].Name));
        highscore_table[i].Score = -999;
    }
}

int high_score::get_score_position(int score)
{
    if (score <= 0)
        return -1;

    for (int position = 0; position < 5; ++position)
    {
        if (highscore_table[position].Score < score)
            return position;
    }

    return -1;
}

void high_score::place_new_score_into(high_score_entry data)
{
    if (data.Position < 0 || data.Position >= 5)
        return;

    for (int i = 4; i > data.Position; --i)
        highscore_table[i] = highscore_table[i - 1];

    data.Entry.Name[31] = 0;
    highscore_table[data.Position] = data.Entry;
}

void high_score::show_high_score_dialog()
{
    // The Xbox port has no ImGui/menu layer. Scores remain available in
    // highscore_table and persist in D:\highscore.dat.
    ShowDialog = false;
}

void high_score::show_and_set_high_score_dialog(high_score_entry score)
{
    /*
     * Xbox release behavior:
     * pb::end_game() already supplies the core Player 1/2/3/4 display name.
     * Commit the score immediately instead of queuing an ImGui name-entry popup.
     */
    if (score.Position < 0 || score.Position >= 5)
        score.Position = get_score_position(score.Entry.Score);

    if (score.Position >= 0 && score.Position < 5)
    {
        place_new_score_into(score);

        // Persist immediately so a dashboard exit after game-over cannot lose it.
        write();
    }

    ShowDialog = false;
}

void high_score::RenderHighScoreDialog()
{
    // No desktop ImGui UI on Xbox.
    ShowDialog = false;
}
