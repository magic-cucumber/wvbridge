import { cp, rm, stat } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const docsDirectory = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const repositoryDirectory = resolve(docsDirectory, '..');
const source = resolve(repositoryDirectory, 'build/dokka/html');
const destination = resolve(docsDirectory, 'public/dokka');

try {
	await stat(source);
} catch {
	throw new Error(
		`Dokka HTML was not found at ${source}. Run \"./gradlew dokkaGenerate\" from the repository root before building the documentation site.`,
	);
}

await rm(destination, { recursive: true, force: true });
await cp(source, destination, { recursive: true });

console.log(`Copied Dokka HTML from ${source} to ${destination}`);
