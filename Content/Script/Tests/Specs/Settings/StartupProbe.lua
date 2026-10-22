-- Used by UnLuaNova.Settings.General: the only way to observe StartupModuleName is that
-- this file runs. It sets a global rather than doing anything observable, so the case can
-- assert "the module ran" without depending on timing or on a side effect.
_G.SpecStartupProbeRan = true
