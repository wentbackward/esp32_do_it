# Click behavior
## Problems
The current behavior is too eager to click and click-drag. 
## Desired behavior
- All parameters should be configurable
- The default mode should be no touch for a period, e.g. 500ms.
- The default behavior should be to move, so any initial touch and hold / movement should be considered move. This behavior should not change from the default mode so subsquent touching, holding or moving should just be translated as a move.
- There should be an anti-wiggle to prevent the cursor from jumping around.
- A tap is a very short touch and *release* within a very small area of movement (say <150ms, 2px)
- A tap should be considered a left click.
- There should be a small window between taps that chains taps and holds together.
  - tap, tap within the window = double tap
  - tap, tap, tap, each within the window = triple tap
  - tap, tap, tap, tap each within the window = quadruple tap
  - A tap, then touch and hold within the window is left click and hold until released. Moving without releasing is dragging.
- Movement acceleration should feel natural and progressive, slow movements should be accurate, faster movement should accelerate
## Tests
Tests should be adjusted/written for these requirements
