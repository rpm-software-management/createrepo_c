
__all__ = ["DeltaRepoError", "DeltaRepoPluginError"]

class DeltaRepoError(Exception):
    pass

class DeltaRepoPluginError(DeltaRepoError):
    pass
