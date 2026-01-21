(define (domain marker_mission)
  (:requirements :strips :typing :equality :negative-preconditions)
  (:types robot waypoint pose marker)

  (:predicates
    ;; robot state
    (at ?r - robot ?w - waypoint)
    (connected ?from - waypoint ?to - waypoint)
    ;; marker information
    (marker-visible-at ?m - marker ?w - waypoint)
    (marker-detected ?m - marker)
    (marker-photographed ?m - marker)

    ;; phase-control flag: only when this is true the photography phase is allowed
    (detection-complete)

    (move-possible ?r - robot)
  )

  ;; Move between connected waypoints
  (:action move
    :parameters (?r - robot ?from - waypoint ?to - waypoint)
    :precondition (and
      (at ?r ?from)
      (move-possible ?r)
      (connected ?from ?to)
    )
    :effect (and
      (not (at ?r ?from))
      (at ?r ?to)
      (not(move-possible ?r))
    )
  )

  ;; Rotate and detect a single marker at this waypoint
  (:action rotateanddetect
    :parameters (?r - robot ?w - waypoint ?m - marker)
    :precondition (and
      (at ?r ?w)
      (marker-visible-at ?m ?w)
    )
    :effect (and
      (marker-detected ?m)
      (move-possible ?r)
    )
  )

  ;; This action becomes possible only once all specified markers are detected.
  ;; It sets the phase flag allowing photographs to be taken.
  (:action finishdetection
    :parameters (?r -robot ?m1 - marker ?m2 - marker ?m3 -marker ?m4 -marker ?w1 - waypoint ?w2 - waypoint ?w3 - waypoint ?w4 - waypoint ?st - waypoint)
    :precondition (and
      ;; NOTE: list here every marker that must be detected before photographing
      (marker-detected ?m1)
      (marker-detected ?m2)
      (marker-detected ?m3)
      (marker-detected ?m4)
      (not (= ?m1 ?m2))
     (not (= ?m1 ?m3))
     (not (= ?m1 ?m4))
     (not (= ?m2 ?m3))
     (not (= ?m2 ?m4))
     (not (= ?m3 ?m4))

     (marker-visible-at ?m1 ?w1)
     (marker-visible-at ?m2 ?w2)
     (marker-visible-at ?m3 ?w3)
     (marker-visible-at ?m4 ?w4)


     (not (= ?w1 ?w2))
     (not (= ?w1 ?w3))
     (not (= ?w1 ?w4))
     (not (= ?w2 ?w3))
     (not (= ?w2 ?w4))
     (not (= ?w3 ?w4))
     (not (= ?w4 ?st))
     (not (= ?w3 ?st))
     (not (= ?w1 ?st))
     (not (= ?w2 ?st))

     
    )
    :effect (and
      (detection-complete)
      (not (at ?r ?w1))
      (not (at ?r ?w2))
      (not (at ?r ?w3))
      (not (at ?r ?w4))
      (at ?r ?st)
      (not(marker-detected ?m1))
     (not(marker-detected ?m2))
     (not(marker-detected ?m3))
     (not(marker-detected ?m4))
    )
  )

  ;; Photograph a single marker (only allowed after detection phase is finished)
  (:action photographmarker
    :parameters (?r - robot ?w - waypoint ?m - marker)
    :precondition (and
      (at ?r ?w)
      (marker-detected ?m)
      (marker-visible-at ?m ?w)
      (detection-complete)
    )
    :effect (and
      (marker-photographed ?m)
    )
  )
)
