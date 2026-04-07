# Vendor Setup

## Purpose

This document describes how the upstream vendor trees are added and maintained in this repository.

The project keeps these upstreams under `third_party/`:

- `picocalc-text-starter`
- `sdltrs`

## Submodule Commands

From the repository root:

```bash
git submodule add https://github.com/BlairLeduc/picocalc-text-starter third_party/picocalc-text-starter
git submodule add https://gitlab.com/jengun/sdltrs third_party/sdltrs
git submodule update --init --recursive
```

## Fresh Clone Setup

After cloning this repository:

```bash
git submodule update --init --recursive
```

## Current Expected Layout

```text
third_party/
├── picocalc-text-starter/
└── sdltrs/
```

## Workflow Rules

- do not place local project code in `third_party/`
- do not edit vendor files directly during normal development
- keep all integration code under `firmware/`
- if an upstream workaround becomes unavoidable, use a documented patch workflow instead of ad hoc edits

## Updating Vendors

To update a submodule to a newer upstream revision:

```bash
git submodule update --remote third_party/picocalc-text-starter
git submodule update --remote third_party/sdltrs
```

Then review the changes and commit the updated submodule pointers.

## Useful Checks

```bash
git submodule status
git status
```

## Notes

- adding the submodules creates a root-level `.gitmodules` file
- Git records the vendor directories as submodule entries rather than ordinary tracked directories
- local scaffold placeholders under `third_party/` should be removed before submodules are added
