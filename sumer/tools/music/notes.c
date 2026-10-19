// Prints the notes a part of a track plays: notes <mood> <part> <first note> <how many>
#include "stubs.h"
static const char *NAMES[] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
int main(int argc, char **argv)
{
    int m = atoi(argv[1]), k = atoi(argv[2]), from = atoi(argv[3]), n = atoi(argv[4]);
    Prepare();
    Part *pt = &parts[m][k];
    printf("%s part %d: %d notes, %.0f beats\n", TRACKS[m].name, k, pt->count, pt->length);
    for (int i = from; i < from + n && i < pt->count; i++) {
        Note *nt = &pt->notes[i];
        printf("  beat %6.2f dur %4.2f %c", nt->beat, nt->dur, nt->kind);
        for (int q = 0; q < nt->nf; q++) {
            double midi = 69 + 12 * log2(nt->freq[q] / 440.0);
            int mi = (int)floor(midi + 0.5);
            printf("  %7.1f Hz (%s%d %+3.0fc)", nt->freq[q], NAMES[((mi % 12) + 12) % 12], mi / 12 - 1, (midi - mi) * 100);
        }
        if (nt->grace > 0) printf("  grace %.1f", nt->grace);
        if (nt->slide) printf("  slide");
        printf("\n");
    }
    return 0;
}
