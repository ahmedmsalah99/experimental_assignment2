# TODO List for Marker Mission Implementation

## Phase 1: Fix Existing Action Nodes
- [ ] 1. Fix `move_action_node_3.cpp` - update waypoint coords to (-6,-6), (-6,6), (6,-6), (6,6), add world node check for lowest ID marker
- [ ] 2. Fix `rotate_and_detect_action_node.cpp` - store robot pose (orientation) not marker pose when detecting

## Phase 2: Create New Action Nodes
- [ ] 3. Create `align_action_node.cpp` - rotate to center marker in camera view
- [ ] 4. Create `world_node.cpp` - track detected markers (ID + pose), query service

## Phase 3: Update Launch File
- [ ] 5. Update `distributed_actions_assignment.launch.py` - add align and world nodes

## Phase 4: Testing
- [ ] 6. Build and test the implementation

