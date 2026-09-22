import type { LevelDocument, WorldDocument } from '../documents';
import { CURRENT_SCHEMA_VERSION } from '../schemas/version';

type SourceDocument = LevelDocument | WorldDocument;

function sourceVersion(value: unknown, label: string): number {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${label} source document must be an object`);
  }
  const version = (value as Record<string, unknown>).schemaVersion;
  if (!Number.isSafeInteger(version) || (version as number) < 1) {
    throw new Error(`${label} source document has an invalid schemaVersion`);
  }
  return version as number;
}

function migrateCurrent<T extends SourceDocument>(value: unknown, label: string): T {
  const version = sourceVersion(value, label);
  if (version > CURRENT_SCHEMA_VERSION) {
    throw new Error(`${label} source schema version ${version} is newer than supported version ${CURRENT_SCHEMA_VERSION}`);
  }
  if (version < CURRENT_SCHEMA_VERSION) {
    throw new Error(`${label} source schema version ${version} has no registered migration to ${CURRENT_SCHEMA_VERSION}`);
  }
  return value as T;
}

export function migrateLevelSource(value: unknown): LevelDocument {
  return migrateCurrent<LevelDocument>(value, 'Level');
}

export function migrateWorldSource(value: unknown): WorldDocument {
  return migrateCurrent<WorldDocument>(value, 'World');
}
