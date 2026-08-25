/*
 * Minimal portable-C reproductions of three P6 branch-layout folds.
 *
 * These functions are compiler probes, not target-source replacements.  The
 * explicit labels document the source CFG whose `Bcc label; b other` form is
 * canonicalized by the GameCube CodeWarrior 1.2.5 family into one inverse
 * conditional branch.
 */

typedef signed char s8;
typedef unsigned short u16;

typedef struct P6Node {
    struct P6Node* next;
    const char* name;
    void* value;
    u16 references;
} P6Node;

extern int p6_compare(const char* left, const char* right);
extern void p6_sink(int value);
extern void p6_sink_ptr(void* value);

void* p6_regfind(P6Node* registry, const char* name)
{
    P6Node* node = registry->next;

    if (name != 0) {
        while (node != 0) {
            if ((s8)p6_compare(name, node->name) == 0) {
                goto found;
            }
            node = node->next;
        }
    } else {
        node = 0;
    }

found:
    if (node != 0) {
        node->references++;
        return node->value;
    }
    return 0;
}

void p6_tally(int selection)
{
    if (selection >= 0 && selection <= 5) {
        goto valid;
    }
    goto done;

valid:
    p6_sink(selection);
done:
    return;
}

void p6_shared_tail(int first, int second, void* entry)
{
    if (first < 0) {
        goto invalid;
    }
    if (second < 0) {
        goto invalid;
    }
    goto valid;

invalid:
    entry = 0;
valid:
    p6_sink_ptr(entry);
}
