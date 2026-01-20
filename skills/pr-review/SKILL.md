---
name: pr-review
description: Comprehensive pull request review skill that analyzes code changes for quality, correctness, and best practices. Use this skill when reviewing code changes before commits or PRs, or when the user asks for a code review.
---

# PR Review Skill

A comprehensive code review skill that provides multi-faceted analysis of code changes. This skill orchestrates several specialized reviewers to ensure thorough coverage of different quality dimensions.

## When to Use

- Before creating a pull request
- After completing a feature or bug fix
- When reviewing existing code for quality issues
- When the user asks to "review my code" or "check my changes"

## Available Analyzers

This skill includes the following specialized analyzers that can be used individually or as part of a comprehensive review:

| Analyzer | Purpose |
|----------|---------|
| `code-reviewer` | General code quality, style guide adherence, and bug detection |
| `code-simplifier` | Code clarity, maintainability, and simplification opportunities |
| `comment-analyzer` | Documentation accuracy, completeness, and long-term value |
| `pr-test-analyzer` | Test coverage quality and completeness |
| `silent-failure-hunter` | Error handling quality and silent failure detection |
| `type-design-analyzer` | Type design, invariants, and encapsulation quality |

## Usage Instructions

### Quick Review (Unstaged Changes)

For a quick review of current unstaged changes:

1. Run `git diff` to see what files have been modified
2. Apply the **code-reviewer** analyzer to check for bugs and style violations
3. If issues are found, address them and re-run the review

### Comprehensive Review

For a thorough review before creating a PR:

1. **Identify changed files**: Run `git diff --name-only` to list modified files
2. **Run code-reviewer**: Check for bugs, style violations, and CLAUDE.md compliance
3. **Run silent-failure-hunter**: Examine all error handling for silent failures
4. **Run comment-analyzer**: Verify documentation accuracy (if comments were added/modified)
5. **Run type-design-analyzer**: Review any new types introduced (if applicable)
6. **Run pr-test-analyzer**: Evaluate test coverage quality (if tests were added/modified)
7. **Run code-simplifier**: Suggest simplification opportunities

### Focused Review

For specific concerns, run only the relevant analyzer:

- **Bug hunting**: Use `code-reviewer` 
- **Error handling audit**: Use `silent-failure-hunter`
- **Documentation check**: Use `comment-analyzer`
- **Test coverage**: Use `pr-test-analyzer`
- **Type design**: Use `type-design-analyzer`
- **Code clarity**: Use `code-simplifier`

## Review Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│                     PR Review Workflow                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. Identify Scope                                              │
│     └─► git diff / git diff --staged / specified files          │
│                                                                 │
│  2. Core Quality Check                                          │
│     └─► code-reviewer (bugs, style, CLAUDE.md compliance)       │
│                                                                 │
│  3. Error Handling Audit                                        │
│     └─► silent-failure-hunter (catch blocks, fallbacks)         │
│                                                                 │
│  4. Specialized Analysis (as needed)                            │
│     ├─► comment-analyzer (if docs changed)                      │
│     ├─► type-design-analyzer (if new types)                     │
│     ├─► pr-test-analyzer (if tests changed)                     │
│     └─► code-simplifier (for cleanup suggestions)               │
│                                                                 │
│  5. Generate Report                                             │
│     └─► Consolidated findings with severity ratings             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Output Format

Each analyzer provides findings in a structured format:

- **Location**: File path and line numbers
- **Severity**: CRITICAL / HIGH / MEDIUM / LOW
- **Issue Description**: What's wrong and why it matters
- **Recommendation**: Specific fix or improvement suggestion
- **Confidence Score**: 0-100 rating (only issues ≥80 are reported)

## Project Integration

The reviewers respect project-specific guidelines from:

- `CLAUDE.md` - Project coding standards and conventions
- Build/lint configurations - Style and formatting rules
- Test conventions - Testing patterns and requirements

## Example Usage

**User**: "Review my recent changes"

**Process**:
1. Run `git diff` to identify changed files
2. Apply `code-reviewer` to all changed `.c` and `.h` files
3. Apply `silent-failure-hunter` if any error handling code was modified
4. Report findings grouped by severity

**User**: "Check if my error handling is correct in the new API client"

**Process**:
1. Focus on the specified files (API client)
2. Apply `silent-failure-hunter` for comprehensive error handling audit
3. Report all silent failure risks and inadequate error handling
