# Graph Report - .  (2026-06-04)

## Corpus Check
- Corpus is ~7,114 words - fits in a single context window. You may not need a graph.

## Summary
- 332 nodes · 492 edges · 17 communities (16 shown, 1 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 55 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_HTTP API & Networking|HTTP API & Networking]]
- [[_COMMUNITY_Status Response Schema|Status Response Schema]]
- [[_COMMUNITY_Peer & Display Schemas|Peer & Display Schemas]]
- [[_COMMUNITY_POST Endpoints|POST Endpoints]]
- [[_COMMUNITY_System Core & Display|System Core & Display]]
- [[_COMMUNITY_Calibration Status Schema|Calibration Status Schema]]
- [[_COMMUNITY_SD Logging Schema|SD Logging Schema]]
- [[_COMMUNITY_IMU Calibration Engine|IMU Calibration Engine]]
- [[_COMMUNITY_GET Endpoints|GET Endpoints]]
- [[_COMMUNITY_API Metadata|API Metadata]]
- [[_COMMUNITY_Delay Config Schema|Delay Config Schema]]
- [[_COMMUNITY_SD Start Schema|SD Start Schema]]
- [[_COMMUNITY_WiFi Tuning Schema|WiFi Tuning Schema]]
- [[_COMMUNITY_VS Code Settings|VS Code Settings]]
- [[_COMMUNITY_VS Code Extensions|VS Code Extensions]]

## God Nodes (most connected - your core abstractions)
1. `paths` - 16 edges
2. `schemas` - 15 edges
3. `responses` - 14 edges
4. `triggerDisplayUpdate()` - 14 edges
5. `setup()` - 13 edges
6. `operationId` - 11 edges
7. `summary` - 11 edges
8. `tags` - 11 edges
9. `drawHeader()` - 11 edges
10. `imuTask()` - 10 edges

## Surprising Connections (you probably didn't know these)
- `setup()` --calls--> `loadCalibrationFromEEPROM()`  [INFERRED]
  src/main.cpp → src/calibration.cpp
- `setup()` --calls--> `registerHttpRoutes()`  [INFERRED]
  src/main.cpp → src/http_handlers.cpp
- `imuTask()` --calls--> `logIMUData()`  [INFERRED]
  src/imu.cpp → src/sd_log.cpp
- `setup()` --calls--> `initSDCard()`  [INFERRED]
  src/main.cpp → src/sd_log.cpp
- `imuTask()` --calls--> `saveCalibrationToEEPROM()`  [INFERRED]
  src/imu.cpp → src/calibration.cpp

## Import Cycles
- None detected.

## Communities (17 total, 1 thin omitted)

### Community 0 - "HTTP API & Networking"
Cohesion: 0.07
Nodes (33): esp_now_send_status_t, triggerDisplayUpdate(), savePeerToEEPROM(), addOrUpdatePeer(), initEspNow(), onEspNowSent(), handlePeerGet(), handlePeerReset() (+25 more)

### Community 1 - "Status Response Schema"
Cohesion: 0.05
Nodes (43): description, type, description, type, description, type, description, type (+35 more)

### Community 2 - "Peer & Display Schemas"
Cohesion: 0.05
Nodes (43): schemas, type, properties, type, description, type, properties, type (+35 more)

### Community 3 - "POST Endpoints"
Cohesion: 0.18
Nodes (30): description, post, post, post, post, paths, /display/toggle, /imu/recalibrate (+22 more)

### Community 4 - "System Core & Display"
Cohesion: 0.15
Nodes (23): btConfigPage(), calibratingPage(), calibrationFailedPage(), calibrationFittingPage(), calibrationInstructionPage(), calibrationSuccessPage(), connectingWifiPage(), displayTask() (+15 more)

### Community 5 - "Calibration Status Schema"
Cohesion: 0.07
Nodes (27): description, enum, type, properties, type, properties, type, description (+19 more)

### Community 6 - "SD Logging Schema"
Cohesion: 0.08
Nodes (26): description, type, description, type, description, type, type, current_file (+18 more)

### Community 7 - "IMU Calibration Engine"
Cohesion: 0.13
Nodes (17): CalibState, applyCalibration(), beginCalibration(), collectCalibSample(), computeEigenvalues3x3(), computeEigenvector3x3(), getCalibState(), loadCalibrationFromEEPROM() (+9 more)

### Community 8 - "GET Endpoints"
Cohesion: 0.15
Nodes (23): content, description, content, description, content, application/json, description, operationId (+15 more)

### Community 9 - "API Metadata"
Cohesion: 0.18
Nodes (10): components, name, info, contact, description, title, version, openapi (+2 more)

### Community 10 - "Delay Config Schema"
Cohesion: 0.22
Nodes (9): properties, type, description, example, maximum, minimum, type, imu_delay_ms (+1 more)

### Community 11 - "SD Start Schema"
Cohesion: 0.22
Nodes (9): description, example, type, type, file, logging_started, SDStartResponse, properties (+1 more)

### Community 12 - "WiFi Tuning Schema"
Cohesion: 0.33
Nodes (6): properties, type, wifi, NetTuneResponse, enum, type

### Community 13 - "VS Code Settings"
Cohesion: 0.50
Nodes (3): files.associations, cmath, *.tcc

## Knowledge Gaps
- **117 isolated node(s):** `recommendations`, `unwantedRecommendations`, `*.tcc`, `cmath`, `openapi` (+112 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `schemas` connect `Peer & Display Schemas` to `Status Response Schema`, `Calibration Status Schema`, `SD Logging Schema`, `API Metadata`, `Delay Config Schema`, `SD Start Schema`, `WiFi Tuning Schema`?**
  _High betweenness centrality (0.338) - this node is a cross-community bridge._
- **Why does `properties` connect `Status Response Schema` to `Delay Config Schema`, `Peer & Display Schemas`, `SD Logging Schema`?**
  _High betweenness centrality (0.189) - this node is a cross-community bridge._
- **Why does `components` connect `API Metadata` to `Peer & Display Schemas`?**
  _High betweenness centrality (0.188) - this node is a cross-community bridge._
- **Are the 12 inferred relationships involving `triggerDisplayUpdate()` (e.g. with `onEspNowSent()` and `handlePeerReset()`) actually correct?**
  _`triggerDisplayUpdate()` has 12 INFERRED edges - model-reasoned connections that need verification._
- **What connects `recommendations`, `unwantedRecommendations`, `*.tcc` to the rest of the system?**
  _117 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `HTTP API & Networking` be split into smaller, more focused modules?**
  _Cohesion score 0.0708245243128964 - nodes in this community are weakly interconnected._
- **Should `Status Response Schema` be split into smaller, more focused modules?**
  _Cohesion score 0.046511627906976744 - nodes in this community are weakly interconnected._