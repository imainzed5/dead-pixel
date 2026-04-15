---
name: code-auditor
description: "Audit a codebase against its design and state docs, identify architecture gaps, missing systems, and blockers, then turn the audit into a dependency-ordered implementation plan. Use for repo audits, current-state reviews, and planning after audit results."
argument-hint: "Audit the repo and generate an implementation plan from the findings"
user-invocable: true
disable-model-invocation: false
---

# Code Auditor

Use this skill when you need a reality-check audit of the repository and then a concrete plan based on what the audit found.

## Workflow

1. Audit the current base first.
2. Compare implementation against the project’s source-of-truth docs.
3. Separate what is working, what is partial, what is missing, and what is risky.
4. Turn the audit into a dependency-ordered plan with clear exit criteria.

## Audit Phase

Start by reading the most relevant project docs and then inspect the codebase to verify what is actually implemented.

Use the audit to answer these questions:
- What is the current build/runtime state?
- What systems are implemented now?
- What systems are present but rough, partial, or mismatched with the target design?
- What is still missing from the intended design or milestone path?
- What architecture or sequencing problems would block the next stage of work?

Prefer evidence over speculation. If the repository does not contain enough proof for a claim, say so explicitly.

When auditing this project, prioritize:
- [docs/dead_pixel_current_state_audit.md](../../../docs/dead_pixel_current_state_audit.md)
- [docs/dead_pixel_survival_gdd_v0.3.md](../../../docs/dead_pixel_survival_gdd_v0.3.md)
- [docs/milestone_backlog.md](../../../docs/milestone_backlog.md)

## Planning Phase

After the audit, generate a plan from the findings rather than from assumptions.

The plan should:
- Respect dependencies and sequencing.
- Start with foundation or architecture fixes before feature breadth.
- Include explicit exit criteria for each phase or milestone.
- Call out verification steps where possible.
- Name the files or subsystems that are most likely to change.
- Keep the plan aligned with the project’s existing milestone style and current-state framing.

If the audit uncovers blockers, put them at the front of the plan and make the dependency chain explicit.

### Audit
- Current state summary
- Implemented now
- Partial or rough areas
- Missing systems
- Key blockers and risks

### Plan
- Ordered phases or milestones
- Dependencies between phases
- Exit criteria for each phase
- Verification notes
- First recommended next step

## Guardrails

- Do not jump straight to implementation ideas before the audit is complete.
- Do not invent status that is not supported by the repository.
- Do not flatten architectural blockers into generic todo items.
- Keep the plan small enough to be actionable, but complete enough to preserve sequencing.
