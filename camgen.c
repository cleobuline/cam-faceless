// camgen.c — generateur headless de CAM-8, pour Linux/VPS, sans Cocoa.
// Reutilise TEL QUEL le coeur du moteur (cam_core.c, cam_forth.c) qui a
// ete teste et compile independamment de l'app Mac tout au long du
// developpement -- cette independance n'etait pas accidentelle.
//
// Usage :
//   camgen --rule regles/hpp-gas.rule --size 256 --steps 500
//          --seed 30 --every 5 --out frames/
//
// Produit une sequence d'images PPM (P6, couleur) dans le dossier de
// sortie. Pour assembler en video, par exemple avec ffmpeg (non requis
// par ce programme lui-meme, juste une suggestion en aval) :
//   ffmpeg -framerate 30 -i frames/frame_%06d.ppm -c:v libx264 -pix_fmt yuv420p out.mp4

#include "cam_core.h"
#include "cam_forth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --rule FICHIER.rule [options]\n"
        "\n"
        "Options :\n"
        "  --rule FICHIER    fichier .rule a compiler (obligatoire)\n"
        "  --size N          taille de grille : 128, 256, 512 ou 1024 (defaut 256)\n"
        "  --steps N         nombre de pas a simuler (defaut 200)\n"
        "  --every N         n'ecrit une image que tous les N pas (defaut 1)\n"
        "  --seed PCT        densite aleatoire de semis, 0-100 (defaut 20)\n"
        "  --seed-plane N    plan a semer, 0-3 (defaut 0)\n"
        "  --out DOSSIER     dossier de sortie des images PPM (defaut ./frames)\n"
        "  --quiet           pas de messages de progression\n"
        "\n"
        "Exemple :\n"
        "  %s --rule regles/dendrite-noise.rule --size 512 --steps 1000 --every 4 --out out/\n",
        prog, prog);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, len, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void dump_ppm(CAMState *cam, const char *path, int visible_mask) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "impossible d'ecrire %s\n", path); return; }
    fprintf(f, "P6\n%u %u\n255\n", cam->width, cam->height);
    uint32_t n = cam->width * cam->height;
    uint8_t *row = malloc(n * 3);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t r, g, b;
        cam_palette(
            (visible_mask & 1) ? cam->plane0_a[i] : 0,
            (visible_mask & 2) ? cam->plane1_a[i] : 0,
            (visible_mask & 4) ? cam->plane2_a[i] : 0,
            (visible_mask & 8) ? cam->plane3_a[i] : 0,
            &r, &g, &b);
        row[i*3+0] = r; row[i*3+1] = g; row[i*3+2] = b;
    }
    fwrite(row, 1, n * 3, f);
    free(row);
    fclose(f);
}

static CAMGridSize parse_size(int n) {
    switch (n) {
        case 128: return CAM_SIZE_128;
        case 256: return CAM_SIZE_256;
        case 512: return CAM_SIZE_512;
        case 1024: return CAM_SIZE_1024;
        default:
            fprintf(stderr, "taille %d non supportee -- utilisation de 256\n", n);
            return CAM_SIZE_256;
    }
}

int main(int argc, char **argv) {
    const char *rule_path = NULL;
    int size = 256;
    int steps = 200;
    int every = 1;
    int seed_pct = 20;
    int seed_plane = 0;
    const char *out_dir = "frames";
    int quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rule") && i+1 < argc) rule_path = argv[++i];
        else if (!strcmp(argv[i], "--size") && i+1 < argc) size = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--every") && i+1 < argc) every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) seed_pct = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed-plane") && i+1 < argc) seed_plane = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i+1 < argc) out_dir = argv[++i];
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "option inconnue : %s\n", argv[i]); usage(argv[0]); return 1; }
    }

    if (!rule_path) { fprintf(stderr, "--rule est obligatoire\n\n"); usage(argv[0]); return 1; }
    if (seed_plane < 0 || seed_plane > 3) { fprintf(stderr, "--seed-plane doit etre entre 0 et 3\n"); return 1; }

    char *src = read_file(rule_path);
    if (!src) { fprintf(stderr, "impossible de lire %s\n", rule_path); return 1; }

    ForthVM vm; forth_init(&vm);
    ForthTables tables; memset(&tables, 0, sizeof(tables));
    int built = forth_compile(&vm, &tables, src);
    free(src);

    if (built == 0) {
        fprintf(stderr, "echec de compilation de la regle\n");
        return 1;
    }
    cam_apply_forth_tables(&tables, built);

    CAMState *cam = cam_create(parse_size(size));
    if (!cam) { fprintf(stderr, "echec d'allocation de la grille\n"); return 1; }

    // semis aleatoire sur le plan demande
    srand((unsigned)time(NULL));
    uint8_t *grids[4] = { cam->plane0_a, cam->plane1_a, cam->plane2_a, cam->plane3_a };
    uint32_t total = cam->width * cam->height;
    for (uint32_t i = 0; i < total; i++)
        grids[seed_plane][i] = (rand() % 100 < seed_pct) ? 1 : 0;

    mkdir(out_dir, 0755); // ignore l'erreur si le dossier existe deja

    if (!quiet) {
        fprintf(stderr, "camgen : %s, grille %dx%d, %d pas (image tous les %d), semis %d%% sur plan %d\n",
                rule_path, cam->width, cam->height, steps, every, seed_pct, seed_plane);
    }

    int frame_num = 0;
    for (int s = 0; s <= steps; s++) {
        if (s % every == 0) {
            char path[512];
            snprintf(path, sizeof(path), "%s/frame_%06d.ppm", out_dir, frame_num++);
            dump_ppm(cam, path, 0xF); // les 4 plans visibles, comme la palette par defaut
            if (!quiet && frame_num % 20 == 0) fprintf(stderr, "  pas %d/%d (%d images ecrites)\n", s, steps, frame_num);
        }
        cam_step(cam);
    }

    if (!quiet) fprintf(stderr, "termine : %d images dans %s/\n", frame_num, out_dir);

    cam_destroy(cam);
    return 0;
}
