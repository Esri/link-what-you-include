<!--
Copyright (c) 2025 Environmental Systems Research Institute, Inc.
SPDX-License-Identifier: Apache-2.0
-->

# lwyi-config

## Introduction

Running link-what-you-include against an old code base is likely to generate a
lot of errors at first. Fixing all these errors at once can be a significant
challenge and it may be reasonable to never fix all of the errors. In order to
allow some targets to not be "link-what-you-include clean", a configuration
file can be provided to limit what is expected of some targets, or to
completely exclude some target from verification.

An [example config file](lwyi-config.json) exists for this project.

## Format

There is a [schema file](doc/lwyi-config.schema.json) describing the expected
fields. The configuration file is a JSON document with an object as the root:

```json
{
  "pedantic": false,
  "targets": {
    "target_a": {
      "allow_includes": ["target_q", "target_r"],
      "allow_links": ["target_s"],
      "interface_include_prefixes": ["target_a_prefix"]
    },
    "target_b": {
      "allow_includes": true,
      "interface_allow_includes": true
    },
    "target_c": {
      "ignore": true
    }
  }
}
```

The root object recognizes the following fields:

### `pedantic`

An optional boolean that controls whether validation enforces strict coherence
of the include and link dependencies. If `false`, this currently means that a
target is permitted to link to a dependency with PUBLIC scope when only
INTERFACE scope is required. The default is `false`.

### `targets`

An optional object providing target specific [configuration
objects](#target-config), keyed by target name.

## Target Config

Each key-value pair in the `targets` object maps a target name to a JSON object
that may contain the following fields:

### `allow_includes`

An optional list of targets this target may include without linking, or a
boolean controlling if all include validation is skipped. An empty array is
equivalent to `false`. The default is `false`.

### `allow_links`

An optional list of targets this target may link without including, or a
boolean indicating if all link validation is skipped. An empty array is
equivalent to `false`. The default is `false`.

### `interface_allow_includes`

An optional boolean controlling if all include validation of usages of this
target is skipped. If `true`, lwyi will allow users of this target to include
it without linking to it. The default is `false`.

### `interface_allow_links`

An optional boolean indicating if all link validation of usages of this target
is skipped. If true, lwyi will allow users of this target to link to it without
including it. The default is `false`.

### `interface_include_prefixes`

An optional array of interface include prefixes for this target, which
disambiguates headers belonging to targets with overlapping interface include
directories. The default is `[]`.

