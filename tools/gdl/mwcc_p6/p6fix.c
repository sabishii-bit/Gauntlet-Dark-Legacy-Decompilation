#include <stdint.h>

/* GC/1.2.5 PCode layouts validated by the inspiredrobot captures. */
typedef struct Block Block;
typedef struct Link Link;
typedef struct Insn Insn;

#pragma pack(push, 1)
struct Link {
    Link* next;
    Block* block;
};

struct Insn {
    Insn* next;          /* +0x00 */
    Insn* previous;      /* +0x04 */
    Block* block;        /* +0x08 */
    uint8_t pad0c[8];
    int16_t opcode;      /* +0x14 */
    uint32_t flags;      /* +0x16 */
    int16_t operand_count; /* +0x1a */
    uint8_t operands[1]; /* +0x1c, 12 bytes each */
};

struct Block {
    Block* next;         /* +0x00 */
    Block* previous;     /* +0x04 */
    uint8_t* label;      /* +0x08; label +4 points back to the block */
    Link* predecessors;  /* +0x0c */
    Link* successors;    /* +0x10 */
    Insn* instructions;  /* +0x14 */
    Insn* last;          /* +0x18 */
    int32_t index;       /* +0x1c */
    uint8_t pad20[12];
    int16_t instruction_count; /* +0x2c */
    int16_t flags;       /* +0x2e */
};
#pragma pack(pop)

enum { OP_B = 0, OP_BT = 5, OP_BF = 8, OPERAND_LABEL = 6 };

typedef struct Candidate {
    Block* conditional;
    Block* body;
    Block* test;
    Block* escape;
    Block* else_block;
    Block* join;
    Insn* conditional_insn;
} Candidate;

static Block* label_target(uint8_t* operand)
{
    uint8_t* label;
    if (operand[0] != OPERAND_LABEL) return 0;
    label = *(uint8_t**)(operand + 2);
    if (!label) return 0;
    return *(Block**)(label + 4);
}

static Block* branch_target(Insn* insn)
{
    if (!insn) return 0;
    if ((insn->opcode == OP_BT || insn->opcode == OP_BF) &&
        insn->operand_count == 3)
        return label_target(insn->operands + 24);
    if (insn->opcode == OP_B && insn->operand_count == 1)
        return label_target(insn->operands);
    return 0;
}

static int valid_terminal(Block* block, Insn* insn)
{
    return block && insn && block->last == insn && insn->block == block &&
           insn->next == 0 && block->instructions &&
           block->instructions->previous == 0;
}

/* Return one only for an exact two-edge set; reject duplicates/cycles. */
static int exact_two(Link* link, Block* a, Block* b)
{
    Block* x;
    Block* y;
    if (!link || !link->next || link->next->next) return 0;
    x = link->block;
    y = link->next->block;
    return x && y && x != y && ((x == a && y == b) || (x == b && y == a));
}

static int exact_one(Link* link, Block* block)
{
    return link && !link->next && link->block == block;
}

/* Prove from successor lists that TARGET's sole predecessor is EXPECTED. */
static int exact_computed_predecessor(Block* head, Block* target,
                                      Block* expected)
{
    Block* block = head;
    int found = 0;
    int block_guard = 0;
    while (block && block_guard++ < 4096) {
        Link* link = block->successors;
        int link_guard = 0;
        int in_block = 0;
        while (link && link_guard++ < 256) {
            if (link->block == target) {
                if (++in_block != 1 || block != expected) return 0;
                ++found;
            }
            link = link->next;
        }
        if (link) return 0;
        block = block->next;
    }
    return !block && found == 1;
}

static Link* find_link(Link* link, Block* block)
{
    int guard = 0;
    while (link && guard++ < 256) {
        if (link->block == block) return link;
        link = link->next;
    }
    return 0;
}

static int inspect_candidate(Block* head, Block* c, Block* escape,
                             Candidate* out)
{
    Block* body;
    Block* join;
    Block* test;
    Block* else_block;
    Block* test_target;
    Block* chain;
    Insn* ci;
    Insn* escape_insn;
    Insn* test_term;
    Insn* compare;
    Insn* else_insn;
    uint16_t vreg;
    int chain_guard = 0;

    if (!c || !escape || !(body = c->next) || escape == body ||
        !(test = escape->previous) || test == c || test->next != escape ||
        !(else_block = escape->next) || else_block == escape ||
        else_block->previous != escape || else_block->next == 0 ||
        else_block->next->previous != else_block)
        return 0;

    ci = c->last;
    if (!valid_terminal(c, ci) ||
        (ci->opcode != OP_BT && ci->opcode != OP_BF) ||
        ci->operand_count != 3 || !(join = branch_target(ci)) ||
        join == c || join == body || join == escape || join == else_block ||
        !body->label || *(Block**)(body->label + 4) != body ||
        !exact_two(c->successors, body, join) ||
        !exact_computed_predecessor(head, body, c))
        return 0;

    escape_insn = escape->last;
    if (escape->instruction_count != 1 || escape->instructions != escape_insn ||
        !valid_terminal(escape, escape_insn) || escape_insn->previous ||
        escape_insn->opcode != OP_B || escape_insn->operand_count != 1 ||
        branch_target(escape_insn) != join ||
        !exact_computed_predecessor(head, escape, test) ||
        !exact_one(escape->successors, join))
        return 0;

    /* Test must prove V==0 on the fallthrough into Escape. */
    test_term = test->last;
    if (!valid_terminal(test, test_term) || test_term->opcode != OP_BF ||
        test_term->operand_count != 3 ||
        !(test_target = branch_target(test_term)) || test_target == escape ||
        !exact_two(test->successors, escape, test_target) ||
        test_term->operands[0] != 3 ||
        test_term->operands[12] != 4 ||
        *(int32_t*)(test_term->operands + 14) != 2)
        return 0;

    compare = test_term->previous;
    if (!compare || compare->opcode != 84 || compare->operand_count != 3 ||
        compare->operands[0] != 3 || compare->operands[12] != 0 ||
        compare->operands[24] != 4 ||
        *(uint16_t*)(compare->operands + 2) !=
            *(uint16_t*)(test_term->operands + 2) ||
        *(int32_t*)(compare->operands + 26) != 0)
        return 0;
    vreg = *(uint16_t*)(compare->operands + 14);

    /* Else is exactly the idempotent assignment LI V,0 -> Join. */
    else_insn = else_block->last;
    if (else_block->instruction_count != 1 ||
        else_block->instructions != else_insn ||
        !valid_terminal(else_block, else_insn) || else_insn->previous ||
        else_insn->opcode != 137 || else_insn->operand_count != 2 ||
        else_insn->operands[0] != 0 ||
        *(uint16_t*)(else_insn->operands + 2) != vreg ||
        else_insn->operands[12] != 4 ||
        *(int32_t*)(else_insn->operands + 14) != 0 ||
        !exact_one(else_block->successors, join) ||
        else_block->next != join || join->previous != else_block)
        return 0;

    /* Body through Test is a straight physical fallthrough chain. */
    chain = body;
    while (chain && chain != escape && chain_guard++ < 4096) {
        Block* next;
        if (chain == test) break;
        next = chain->next;
        if (!next || !exact_one(chain->successors, next)) return 0;
        chain = next;
    }
    if (chain != test || chain_guard >= 4096) return 0;

    out->conditional = c;
    out->body = body;
    out->test = test;
    out->escape = escape;
    out->else_block = else_block;
    out->join = join;
    out->conditional_insn = ci;
    return 1;
}

/*
 * Apply only when the entire function contains exactly one proven topology.
 * This makes the derived compiler fail closed on ambiguous/multi-site input.
 */
void p6fix_scan(Block* head)
{
    Candidate candidate;
    Candidate current;
    Block* c;
    int matches = 0;
    int outer_guard = 0;

    if (head && head->previous) return;
    for (c = head; c && outer_guard++ < 4096; c = c->next) {
        Block* g;
        Block* stop;
        Block* order_probe;
        int inner_guard = 0;
        if (c->next && c->next->previous != c) return;
        if (!c->next || !c->last) continue;
        stop = branch_target(c->last);
        if (!stop) continue;
        /* This rule is only for a forward join in the physical block list. */
        order_probe = c->next;
        while (order_probe && order_probe != stop && inner_guard++ < 4096)
            order_probe = order_probe->next;
        if (inner_guard >= 4096) return;
        if (order_probe != stop) continue;
        inner_guard = 0;
        for (g = c->next->next; g && g != stop && inner_guard++ < 4096;
             g = g->next) {
            if (inspect_candidate(head, c, g, &current)) {
                candidate = current;
                if (++matches != 1) return;
            }
        }
        if (inner_guard >= 4096) return;
    }
    if (c || matches != 1) return;

    {
        Block* c0 = candidate.conditional;
        Block* body = candidate.body;
        Block* test = candidate.test;
        Block* escape = candidate.escape;
        Block* else_block = candidate.else_block;
        Block* j = candidate.join;
        Insn* ci = candidate.conditional_insn;

        /* Move Escape from after Test to immediately after C. */
        c0->next = escape;
        escape->previous = c0;
        escape->next = body;
        body->previous = escape;
        test->next = else_block;
        else_block->previous = test;

        /* Invert C and branch to the old Body block. */
        ci->opcode = (ci->opcode == OP_BT) ? OP_BF : OP_BT;
        *(uint8_t**)(ci->operands + 24 + 2) = body->label;
        find_link(c0->successors, j)->block = escape;
        find_link(test->successors, escape)->block = else_block;
    }
}
