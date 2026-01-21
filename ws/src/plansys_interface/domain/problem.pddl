(define (problem marker_mission_problem)
  (:domain marker_mission)

  (:objects
    robot1 - robot

    st wp1 wp2 wp3 wp4 - waypoint

    m1 m2 m3 m4 - marker
  )

  (:init
    (at robot1 st)
    (move-possible robot1)
    ;; marker visibility hints
    (marker-visible-at m1 wp1)
    (marker-visible-at m2 wp2)
    (marker-visible-at m3 wp3)
    (marker-visible-at m4 wp4)
    ;; explicit connectivity (only legal moves)
    (connected st wp1)
    (connected wp1 st) 
    (connected wp1 wp2)
    (connected wp2 wp1)
    (connected wp2 wp3)
    (connected wp3 wp2)
    (connected wp3 wp4)
    (connected wp4 wp3)
  )

  (:goal
    (and
      (marker-photographed m1)
      (marker-photographed m2)
      (marker-photographed m3)
      (marker-photographed m4)
    )
  )
)
