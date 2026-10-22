---
title: "Test reports"
description: "Where to find the automated test reports, and why they are written in Chinese"
---

# Test reports

Every `Tools\RunTests.ps1` run writes a report per backend; `-Publish` copies the two of them into
`Docs/`, which is what the docs site serves:

- [Test report (Lua backend)](../test-lua-report.md)
- [Test report (Luau backend)](../test-luau-report.md)

**These two pages are generated, in Chinese**, by `Tools/GenerateTestReport.py - they are not
hand-written and they are not translated. The test names come from
`BEGIN_DEFINE_SPEC` / `It(TEXT("..."))` in the C++ specs, and the suite has always named its cases in
Chinese; translating the reports would mean translating the test names, which would break the
`-Filter` examples in [Testing and Automation](Testing.md) and the drift baseline in
`Tools/baseline/*.json`.

So the English navigation links here instead of listing the reports directly. The numbers - pass
rate, totals, per-test durations - are language independent, and the test names can be copied
straight into a `-Filter`.

Regenerate them with:

```powershell
.\Tools\RunTests.ps1 -Backend Both -Publish
```

---

## Further reading

- [Testing and Automation](Testing.md) - how the suite is run and what the report contains
- [Test report (Lua backend)](../test-lua-report.md)
- [Test report (Luau backend)](../test-luau-report.md)
