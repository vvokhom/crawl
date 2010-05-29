/*
 *  File:       tiledoll.cc
 *  Summary:    Region system implementations
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tiledoll.h"

#include <sys/stat.h>

#include "files.h"
#include "tilebuf.h"
#include "tiledef-player.h"
#include "transform.h"

dolls_data::dolls_data()
{
    parts = new tileidx_t[TILEP_PART_MAX];
    memset(parts, 0, TILEP_PART_MAX * sizeof(int));
}

dolls_data::dolls_data(const dolls_data& _orig)
{
    parts = new tileidx_t[TILEP_PART_MAX];
    memcpy(parts, _orig.parts, TILEP_PART_MAX * sizeof(int));
}

const dolls_data& dolls_data::operator=(const dolls_data& other)
{
    memcpy(parts, other.parts, TILEP_PART_MAX * sizeof(int));
    return (*this);
}

dolls_data::~dolls_data()
{
    delete[] parts;
    parts = NULL;
}

dolls_data player_doll;
int doll_gender = -1;

bool save_doll_data(int mode, int num, const dolls_data* dolls)
{
    // Save mode, num, and all dolls into dolls.txt.
    std::string dollsTxtString = datafile_path("dolls.txt", false, true);

    struct stat stFileInfo;
    stat(dollsTxtString.c_str(), &stFileInfo);

    // Write into the current directory instead if we didn't find the file
    // or don't have write permissions.
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0
                              || !(stFileInfo.st_mode & S_IWUSR)) ? "dolls.txt"
                            : dollsTxtString.c_str();

    FILE *fp = NULL;
    if ((fp = fopen(dollsTxt, "w+")) != NULL)
    {
        fprintf(fp, "MODE=%s\n",
                    (mode == TILEP_MODE_EQUIP)   ? "EQUIP" :
                    (mode == TILEP_MODE_LOADING) ? "LOADING"
                                                 : "DEFAULT");

        fprintf(fp, "NUM=%02d\n", num == -1 ? 0 : num);

        // Print some explanatory comments. May contain no spaces!
        fprintf(fp, "#Legend:\n");
        fprintf(fp, "#***:equipment/123:index/000:none\n");
        fprintf(fp, "#Shadow/Base/Cloak/Boots/Legs/Body/Gloves/Weapon/Shield/Hair/Beard/Helmet/Halo/Enchant/DrcHead/DrcWing\n");
        fprintf(fp, "#Sh:Bse:Clk:Bts:Leg:Bdy:Glv:Wpn:Shd:Hai:Brd:Hlm:Hal:Enc:Drc:Wng\n");
        char fbuf[80];
        for (unsigned int i = 0; i < NUM_MAX_DOLLS; ++i)
        {
            tilep_print_parts(fbuf, dolls[i]);
            fprintf(fp, "%s\n", fbuf);
        }
        fclose(fp);

        return (true);
    }

    return (false);
}

bool load_doll_data(const char *fn, dolls_data *dolls, int max,
                    tile_doll_mode *mode, int *cur)
{
    char fbuf[1024];
    FILE *fp  = NULL;

    std::string dollsTxtString = datafile_path(fn, false, true);

    struct stat stFileInfo;
    stat(dollsTxtString.c_str(), &stFileInfo);

    // Try to read from the current directory instead if we didn't find the
    // file or don't have reading permissions.
    const char *dollsTxt = (dollsTxtString.c_str()[0] == 0
                              || !(stFileInfo.st_mode & S_IRUSR)) ? "dolls.txt"
                            : dollsTxtString.c_str();


    if ( (fp = fopen(dollsTxt, "r")) == NULL )
    {
        // File doesn't exist. By default, use equipment settings.
        *mode = TILEP_MODE_EQUIP;
        return (false);
    }
    else
    {
        memset(fbuf, 0, sizeof(fbuf));
        // Read mode from file.
        if (fscanf(fp, "%1023s", fbuf) != EOF)
        {
            if (strcmp(fbuf, "MODE=DEFAULT") == 0)
                *mode = TILEP_MODE_DEFAULT;
            else if (strcmp(fbuf, "MODE=EQUIP") == 0)
                *mode = TILEP_MODE_EQUIP; // Nothing else to be done.
        }
        // Read current doll from file.
        if (fscanf(fp, "%1023s", fbuf) != EOF)
        {
            if (strncmp(fbuf, "NUM=", 4) == 0)
            {
                sscanf(fbuf, "NUM=%d", cur);
                if (*cur < 0 || *cur >= NUM_MAX_DOLLS)
                    *cur = 0;
            }
        }

        if (max == 1)
        {
            // Load only one doll, either the one defined by NUM or
            // use the default/equipment setting.
            if (*mode != TILEP_MODE_LOADING)
            {
                if (doll_gender == -1)
                    doll_gender = coinflip();

                if (*mode == TILEP_MODE_DEFAULT)
                    tilep_job_default(you.char_class, doll_gender, &dolls[0]);

                // If we don't need to load a doll, return now.
                fclose(fp);
                return (true);
            }

            int count = 0;
            while (fscanf(fp, "%1023s", fbuf) != EOF)
            {
                if (fbuf[0] == '#') // Skip comment lines.
                    continue;

                if (*cur == count++)
                {
                    tilep_scan_parts(fbuf, dolls[0], you.species,
                                     you.experience_level);
                    doll_gender = get_gender_from_tile(dolls[0]);
                    break;
                }
            }
        }
        else // Load up to max dolls from file.
        {
            for (int count = 0; count < max && fscanf(fp, "%1023s", fbuf) != EOF; )
            {
                if (fbuf[0] == '#') // Skip comment lines.
                    continue;

                tilep_scan_parts(fbuf, dolls[count++], you.species,
                                 you.experience_level);
            }
        }

        fclose(fp);
        return (true);
    }
}

void init_player_doll()
{
    dolls_data dolls[NUM_MAX_DOLLS];

    for (int i = 0; i < NUM_MAX_DOLLS; i++)
        for (int j = 0; j < TILEP_PART_MAX; j++)
            dolls[i].parts[j] = TILEP_SHOW_EQUIP;

    tile_doll_mode mode = TILEP_MODE_LOADING;
    int cur = 0;
    load_doll_data("dolls.txt", dolls, NUM_MAX_DOLLS, &mode, &cur);

    if (mode == TILEP_MODE_LOADING)
    {
        player_doll = dolls[cur];
        tilep_race_default(you.species, doll_gender, you.experience_level,
                           &player_doll);
        return;
    }

    if (doll_gender == -1)
        doll_gender = coinflip();

    for (int i = 0; i < TILEP_PART_MAX; i++)
        player_doll.parts[i] = TILEP_SHOW_EQUIP;
    tilep_race_default(you.species, doll_gender, you.experience_level, &player_doll);

    if (mode == TILEP_MODE_EQUIP)
        return;

    tilep_job_default(you.char_class, doll_gender, &player_doll);
}

static int _get_random_doll_part(int p)
{
    ASSERT(p >= 0 && p <= TILEP_PART_MAX);
    return (tile_player_part_start[p]
            + random2(tile_player_part_count[p]));
}

static void _fill_doll_part(dolls_data &doll, int p)
{
    ASSERT(p >= 0 && p <= TILEP_PART_MAX);
    doll.parts[p] = _get_random_doll_part(p);
}

void create_random_doll(dolls_data &rdoll)
{
    // All dolls roll for these.
    _fill_doll_part(rdoll, TILEP_PART_BODY);
    _fill_doll_part(rdoll, TILEP_PART_HAND1);
    _fill_doll_part(rdoll, TILEP_PART_LEG);
    _fill_doll_part(rdoll, TILEP_PART_BOOTS);
    _fill_doll_part(rdoll, TILEP_PART_HAIR);

    // The following are only rolled with 50% chance.
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_CLOAK);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_ARM);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HAND2);
    if (coinflip())
        _fill_doll_part(rdoll, TILEP_PART_HELM);

    // Only male dolls get a chance at a beard.
    if (get_gender_from_tile(rdoll) == TILEP_GENDER_MALE && one_chance_in(4))
        _fill_doll_part(rdoll, TILEP_PART_BEARD);
}

void fill_doll_equipment(dolls_data &result)
{
    // Base tile.
    if (result.parts[TILEP_PART_BASE] == TILEP_SHOW_EQUIP)
    {
        if (doll_gender == -1)
            doll_gender = get_gender_from_tile(player_doll);
        tilep_race_default(you.species, doll_gender, you.experience_level,
                           &result);
    }

    // Main hand.
    if (result.parts[TILEP_PART_HAND1] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_WEAPON];
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            result.parts[TILEP_PART_HAND1] = TILEP_HAND1_BLADEHAND;
        else if (item == -1)
            result.parts[TILEP_PART_HAND1] = 0;
        else
            result.parts[TILEP_PART_HAND1] = tilep_equ_weapon(you.inv[item]);
    }
    // Off hand.
    if (result.parts[TILEP_PART_HAND2] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_SHIELD];
        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BLADE_HANDS)
            result.parts[TILEP_PART_HAND2] = TILEP_HAND2_BLADEHAND;
        else if (item == -1)
            result.parts[TILEP_PART_HAND2] = 0;
        else
            result.parts[TILEP_PART_HAND2] = tilep_equ_shield(you.inv[item]);
    }
    // Body armour.
    if (result.parts[TILEP_PART_BODY] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_BODY_ARMOUR];
        if (item == -1)
            result.parts[TILEP_PART_BODY] = 0;
        else
            result.parts[TILEP_PART_BODY] = tilep_equ_armour(you.inv[item]);
    }
    // Cloak.
    if (result.parts[TILEP_PART_CLOAK] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_CLOAK];
        if (item == -1)
            result.parts[TILEP_PART_CLOAK] = 0;
        else
            result.parts[TILEP_PART_CLOAK] = tilep_equ_cloak(you.inv[item]);
    }
    // Helmet.
    if (result.parts[TILEP_PART_HELM] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_HELMET];
        if (item != -1)
        {
            result.parts[TILEP_PART_HELM] = tilep_equ_helm(you.inv[item]);
        }
        else if (player_mutation_level(MUT_HORNS) > 0)
        {
            switch (player_mutation_level(MUT_HORNS))
            {
                case 1:
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS1;
                    break;
                case 2:
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS2;
                    break;
                case 3:
                    result.parts[TILEP_PART_HELM] = TILEP_HELM_HORNS3;
                    break;
            }
        }
        else
        {
            result.parts[TILEP_PART_HELM] = 0;
        }
    }
    // Boots.
    if (result.parts[TILEP_PART_BOOTS] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_BOOTS];
        if (item != -1)
            result.parts[TILEP_PART_BOOTS] = tilep_equ_boots(you.inv[item]);
        else if (player_mutation_level(MUT_HOOVES) >= 3)
            result.parts[TILEP_PART_BOOTS] = TILEP_BOOTS_HOOVES;
        else
            result.parts[TILEP_PART_BOOTS] = 0;
    }
    // Gloves.
    if (result.parts[TILEP_PART_ARM] == TILEP_SHOW_EQUIP)
    {
        const int item = you.equip[EQ_GLOVES];
        if (item != -1)
            result.parts[TILEP_PART_ARM] = tilep_equ_gloves(you.inv[item]);
        else if (you.has_claws(false) >= 3)
            result.parts[TILEP_PART_ARM] = TILEP_ARM_CLAWS;
        else
            result.parts[TILEP_PART_ARM] = 0;
    }
    // Halo.
    if (result.parts[TILEP_PART_HALO] == TILEP_SHOW_EQUIP)
    {
        const bool halo = you.haloed();
        result.parts[TILEP_PART_HALO] = halo ? TILEP_HALO_TSO : 0;
    }
    // Enchantments.
    if (result.parts[TILEP_PART_ENCH] == TILEP_SHOW_EQUIP)
    {
        result.parts[TILEP_PART_ENCH] =
            (you.duration[DUR_LIQUID_FLAMES] ? TILEP_ENCH_STICKY_FLAME : 0);
    }
    // Draconian head/wings.
    if (player_genus(GENPC_DRACONIAN))
    {
        tileidx_t base = 0;
        tileidx_t head = 0;
        tileidx_t wing = 0;
        tilep_draconian_init(you.species, you.experience_level,
                             &base, &head, &wing);

        if (result.parts[TILEP_PART_DRCHEAD] == TILEP_SHOW_EQUIP)
            result.parts[TILEP_PART_DRCHEAD] = head;
        if (result.parts[TILEP_PART_DRCWING] == TILEP_SHOW_EQUIP)
            result.parts[TILEP_PART_DRCWING] = wing;
    }
    // Shadow.
    if (result.parts[TILEP_PART_SHADOW] == TILEP_SHOW_EQUIP)
        result.parts[TILEP_PART_SHADOW] = TILEP_SHADOW_SHADOW;

    // Various other slots.
    for (int i = 0; i < TILEP_PART_MAX; i++)
        if (result.parts[i] == TILEP_SHOW_EQUIP)
            result.parts[i] = 0;
}

// Writes equipment information into per-character doll file.
void save_doll_file(FILE *dollf)
{
    ASSERT(dollf);

    dolls_data result = player_doll;
    fill_doll_equipment(result);

    // Write into file.
    char fbuf[80];
    tilep_print_parts(fbuf, result);
    fprintf(dollf, "%s\n", fbuf);

    if (you.attribute[ATTR_HELD] > 0)
        fprintf(dollf, "net\n");
}

void pack_doll_buf(SubmergedTileBuffer& buf, const dolls_data &doll, int x, int y, bool submerged, bool ghost)
{
    // Ordered from back to front.
    int p_order[TILEP_PART_MAX] =
    {
        // background
        TILEP_PART_SHADOW,
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        // player
        TILEP_PART_BASE,
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_HELM,
        TILEP_PART_HAND1,
        TILEP_PART_HAND2,
        TILEP_PART_DRCHEAD
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(doll, flags);

    // For skirts, boots go under the leg armour.  For pants, they go over.
    if (doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[6] = TILEP_PART_BOOTS;
        p_order[7] = TILEP_PART_LEG;
    }

    // Special case bardings from being cut off.
    const bool is_naga = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_NAGA);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    const bool is_cent = is_player_tile(doll.parts[TILEP_PART_BASE],
                                        TILEP_BASE_CENTAUR);

    if (doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    // A higher index here means that the part should be drawn on top.
    // This is drawn in reverse order because this could be a ghost
    // or being drawn in water, in which case we want the top-most part
    // to blend with the background underneath and not with the parts
    // underneath.  Parts drawn afterwards will not obscure parts drawn
    // previously, because "i" is passed as the depth below.
    for (int i = TILEP_PART_MAX - 1; i >= 0; --i)
    {
        int p = p_order[i];

        if (!doll.parts[p] || flags[p] == TILEP_FLAG_HIDE)
            continue;

        if (p == TILEP_PART_SHADOW && (submerged || ghost))
            continue;

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        buf.add(doll.parts[p], x, y, i, submerged, ghost, 0, 0, ymax);
    }
}

#endif
