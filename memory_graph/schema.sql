PRAGMA foreign_keys = ON;

CREATE TABLE meta (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE record_ingest (
    record_id TEXT PRIMARY KEY,
    record_kind TEXT NOT NULL,
    record_state TEXT NOT NULL,
    source_path TEXT NOT NULL,
    raw_json TEXT NOT NULL
);

CREATE TABLE source_artifact (
    id INTEGER PRIMARY KEY,
    artifact_key TEXT NOT NULL UNIQUE,
    platform TEXT NOT NULL,
    build_name TEXT NOT NULL,
    artifact_kind TEXT NOT NULL,
    path TEXT,
    sha256 TEXT,
    provenance_note TEXT
);

CREATE TABLE document (
    id INTEGER PRIMARY KEY,
    artifact_id INTEGER NOT NULL REFERENCES source_artifact(id),
    path TEXT NOT NULL UNIQUE,
    title TEXT,
    format TEXT NOT NULL,
    document_class TEXT NOT NULL,
    lifecycle_state TEXT NOT NULL,
    sha256 TEXT NOT NULL,
    byte_size INTEGER NOT NULL
);

CREATE TABLE document_chunk (
    id INTEGER PRIMARY KEY,
    document_id INTEGER NOT NULL REFERENCES document(id),
    ordinal INTEGER NOT NULL,
    heading TEXT,
    line_start INTEGER NOT NULL,
    line_end INTEGER NOT NULL,
    content TEXT NOT NULL,
    content_sha256 TEXT NOT NULL,
    normalized_sha256 TEXT NOT NULL,
    UNIQUE(document_id, ordinal)
);

CREATE VIRTUAL TABLE document_chunk_fts USING fts5(
    chunk_id UNINDEXED,
    path UNINDEXED,
    heading,
    content,
    tokenize = 'unicode61 remove_diacritics 2'
);

CREATE VIRTUAL TABLE record_fts USING fts5(
    record_id UNINDEXED,
    record_kind UNINDEXED,
    record_state UNINDEXED,
    body,
    tokenize = 'unicode61 remove_diacritics 2'
);

CREATE TABLE entity (
    id INTEGER PRIMARY KEY,
    entity_key TEXT NOT NULL UNIQUE,
    entity_type TEXT NOT NULL,
    name TEXT NOT NULL,
    state TEXT NOT NULL,
    attributes_json TEXT NOT NULL
);

CREATE TABLE entity_alias (
    entity_id INTEGER NOT NULL REFERENCES entity(id),
    alias TEXT NOT NULL,
    alias_kind TEXT NOT NULL DEFAULT 'name',
    PRIMARY KEY(entity_id, alias, alias_kind)
);

CREATE VIRTUAL TABLE entity_fts USING fts5(
    entity_id UNINDEXED,
    entity_key,
    entity_type,
    name,
    aliases,
    tokenize = 'unicode61 remove_diacritics 2'
);

CREATE TABLE edge (
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL UNIQUE REFERENCES record_ingest(record_id),
    source_entity_id INTEGER NOT NULL REFERENCES entity(id),
    relation TEXT NOT NULL,
    target_entity_id INTEGER NOT NULL REFERENCES entity(id),
    state TEXT NOT NULL,
    note TEXT,
    valid_from TEXT,
    valid_to TEXT,
    superseded_by TEXT REFERENCES record_ingest(record_id)
);

CREATE TABLE claim (
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL UNIQUE REFERENCES record_ingest(record_id),
    subject_entity_id INTEGER NOT NULL REFERENCES entity(id),
    predicate TEXT NOT NULL,
    object_entity_id INTEGER REFERENCES entity(id),
    value_json TEXT,
    epistemic_state TEXT NOT NULL,
    note TEXT,
    valid_from TEXT,
    valid_to TEXT,
    superseded_by TEXT REFERENCES record_ingest(record_id),
    CHECK(object_entity_id IS NOT NULL OR value_json IS NOT NULL)
);

CREATE TABLE evidence (
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL UNIQUE REFERENCES record_ingest(record_id),
    claim_record_id TEXT REFERENCES claim(record_id),
    edge_record_id TEXT REFERENCES edge(record_id),
    evidence_kind TEXT NOT NULL,
    locator TEXT NOT NULL,
    detail TEXT NOT NULL,
    content_sha256 TEXT,
    CHECK(claim_record_id IS NOT NULL OR edge_record_id IS NOT NULL)
);

CREATE TABLE attempt (
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL UNIQUE REFERENCES record_ingest(record_id),
    function_entity_id INTEGER NOT NULL REFERENCES entity(id),
    tu_entity_id INTEGER REFERENCES entity(id),
    compiler_entity_id INTEGER REFERENCES entity(id),
    source_revision TEXT,
    attempted_axis TEXT NOT NULL,
    outcome TEXT NOT NULL,
    residual_class TEXT,
    semantic_note TEXT,
    commit_hash TEXT,
    started_at TEXT,
    finished_at TEXT
);

CREATE TABLE measurement (
    id INTEGER PRIMARY KEY,
    attempt_record_id TEXT NOT NULL REFERENCES attempt(record_id),
    phase TEXT NOT NULL,
    target_instructions INTEGER,
    current_instructions INTEGER,
    fuzzy_percent REAL,
    real_diffs INTEGER,
    frame_size INTEGER,
    project_fuzzy_percent REAL,
    project_matched_percent REAL,
    UNIQUE(attempt_record_id, phase)
);

CREATE TABLE work_claim (
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL UNIQUE REFERENCES record_ingest(record_id),
    function_entity_id INTEGER NOT NULL REFERENCES entity(id),
    owner TEXT NOT NULL,
    branch TEXT,
    worktree TEXT,
    state TEXT NOT NULL,
    claimed_at TEXT NOT NULL,
    released_at TEXT
);

CREATE TABLE tool_catalog (
    id INTEGER PRIMARY KEY,
    record_id TEXT UNIQUE REFERENCES record_ingest(record_id),
    tool_key TEXT NOT NULL,
    name TEXT NOT NULL,
    tool_kind TEXT NOT NULL,
    source_path TEXT,
    entrypoint TEXT,
    status TEXT NOT NULL,
    purpose TEXT NOT NULL,
    usage_json TEXT NOT NULL,
    constraints_json TEXT NOT NULL,
    attributes_json TEXT NOT NULL,
    source_kind TEXT NOT NULL,
    supersedes TEXT REFERENCES record_ingest(record_id),
    valid_from TEXT,
    valid_to TEXT
);

CREATE INDEX tool_catalog_key_idx ON tool_catalog(tool_key, status);

CREATE VIRTUAL TABLE tool_catalog_fts USING fts5(
    tool_id UNINDEXED,
    tool_key,
    name,
    source_path,
    purpose,
    usage,
    constraints,
    tokenize = 'unicode61 remove_diacritics 2'
);

CREATE TABLE binary_module (
    id INTEGER PRIMARY KEY,
    artifact_id INTEGER NOT NULL REFERENCES source_artifact(id),
    platform TEXT NOT NULL,
    object_name TEXT NOT NULL,
    source_name TEXT,
    module_ordinal INTEGER,
    UNIQUE(artifact_id, object_name)
);

CREATE TABLE binary_symbol (
    id INTEGER PRIMARY KEY,
    module_id INTEGER REFERENCES binary_module(id),
    artifact_id INTEGER NOT NULL REFERENCES source_artifact(id),
    platform TEXT NOT NULL,
    symbol_kind TEXT NOT NULL,
    scope TEXT,
    raw_name TEXT NOT NULL,
    normalized_name TEXT NOT NULL,
    section TEXT,
    segment INTEGER,
    offset INTEGER,
    address INTEGER,
    size INTEGER,
    source_ordinal INTEGER,
    record_type INTEGER
);

CREATE INDEX binary_symbol_name_idx
    ON binary_symbol(platform, normalized_name);
CREATE INDEX binary_symbol_address_idx
    ON binary_symbol(platform, address);
CREATE INDEX binary_symbol_module_order_idx
    ON binary_symbol(module_id, source_ordinal);

CREATE VIRTUAL TABLE binary_symbol_fts USING fts5(
    symbol_id UNINDEXED,
    platform UNINDEXED,
    module_name,
    raw_name,
    normalized_name,
    tokenize = 'unicode61 remove_diacritics 2'
);

CREATE TABLE cross_platform_symbol_link (
    id INTEGER PRIMARY KEY,
    gcn_symbol_id INTEGER NOT NULL REFERENCES binary_symbol(id),
    xbox_symbol_id INTEGER NOT NULL REFERENCES binary_symbol(id),
    relation TEXT NOT NULL,
    verification TEXT NOT NULL,
    confidence REAL,
    method TEXT NOT NULL,
    note TEXT,
    valid_from TEXT,
    valid_to TEXT,
    superseded_by INTEGER REFERENCES cross_platform_symbol_link(id),
    UNIQUE(gcn_symbol_id, xbox_symbol_id, relation, method)
);

CREATE TABLE symbol_link_evidence (
    id INTEGER PRIMARY KEY,
    link_id INTEGER NOT NULL REFERENCES cross_platform_symbol_link(id),
    evidence_kind TEXT NOT NULL,
    locator TEXT NOT NULL,
    detail TEXT NOT NULL,
    weight INTEGER
);

CREATE TABLE pdb_type (
    id INTEGER PRIMARY KEY,
    artifact_id INTEGER NOT NULL REFERENCES source_artifact(id),
    name TEXT NOT NULL,
    type_kind TEXT NOT NULL,
    size INTEGER,
    category TEXT,
    source_file TEXT,
    UNIQUE(artifact_id, name, category)
);

CREATE TABLE pdb_field (
    id INTEGER PRIMARY KEY,
    type_id INTEGER NOT NULL REFERENCES pdb_type(id),
    name TEXT NOT NULL,
    field_type TEXT,
    byte_offset INTEGER,
    bit_offset INTEGER,
    bit_size INTEGER,
    byte_size INTEGER,
    array_count INTEGER
);

CREATE INDEX pdb_type_name_idx ON pdb_type(name);
CREATE INDEX pdb_field_name_idx ON pdb_field(name);

CREATE TABLE cross_platform_type_link (
    id INTEGER PRIMARY KEY,
    pdb_type_id INTEGER NOT NULL REFERENCES pdb_type(id),
    gcn_type_entity_id INTEGER NOT NULL REFERENCES entity(id),
    verification TEXT NOT NULL,
    layout_status TEXT NOT NULL,
    verified_size INTEGER,
    note TEXT,
    valid_from TEXT,
    valid_to TEXT,
    UNIQUE(pdb_type_id, gcn_type_entity_id, valid_from)
);

CREATE TABLE field_verification (
    id INTEGER PRIMARY KEY,
    type_link_id INTEGER NOT NULL REFERENCES cross_platform_type_link(id),
    pdb_field_id INTEGER NOT NULL REFERENCES pdb_field(id),
    gcn_offset INTEGER,
    status TEXT NOT NULL,
    evidence_locator TEXT NOT NULL,
    note TEXT
);

CREATE TABLE migration_proposal (
    id INTEGER PRIMARY KEY,
    proposal_key TEXT NOT NULL UNIQUE,
    proposal_kind TEXT NOT NULL,
    subject_key TEXT,
    title TEXT NOT NULL,
    payload_json TEXT NOT NULL,
    source_document_id INTEGER NOT NULL REFERENCES document(id),
    line_start INTEGER NOT NULL,
    line_end INTEGER NOT NULL,
    review_state TEXT NOT NULL DEFAULT 'pending'
);

CREATE INDEX migration_proposal_subject_idx
    ON migration_proposal(subject_key, review_state);
