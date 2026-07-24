"""PC-side research ingest — unplugged lessons → local learning store."""

from .plug_sync import ingest_unplugged_lessons, PlugSyncResult

__all__ = ["ingest_unplugged_lessons", "PlugSyncResult"]
