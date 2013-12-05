
__all__ = ["DeltaRepoError", "DeltaRepoPluginError"]

class DeltaRepoError(Exception):
    """Exception raised by deltarepo library"""
    pass

class DeltaRepoPluginError(DeltaRepoError):
    """Exception raised by delta plugins of deltarepo library"""
    pass
