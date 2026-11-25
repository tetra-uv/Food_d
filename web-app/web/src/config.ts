// Frontend configuration. Tiles are only used in GEO mode (when /network carries a `geo` overlay).
// Public OSM tile servers are for light/demo use only (see https://operations.osmfoundation.org/policies/tiles/);
// point TILE_URL at your own tile source for anything heavier.
export const TILE_URL: string =
  (import.meta.env.VITE_TILE_URL as string | undefined) || 'https://tile.openstreetmap.org/{z}/{x}/{y}.png';
export const TILE_ATTRIBUTION =
  '&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noreferrer">OpenStreetMap</a> contributors';

/** Debounce for what-if previews (latest request wins). */
export const PREVIEW_DEBOUNCE_MS = 150;
