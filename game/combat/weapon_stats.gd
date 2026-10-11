class_name WeaponStats
extends Resource
## How one kind of weapon behaves. Each weapon is a resource in
## combat/weapons/, so it can be tuned without touching code.

## Short name used on the network and in settings, such as "blaster".
@export var id := ""
@export var display_name := ""
## Damage per hit, per pellet for a weapon with several.
@export var damage := 20.0
@export var headshot_multiplier := 1.75
## Seconds between shots.
@export var fire_interval := 0.12
## Whether holding the trigger keeps firing.
@export var automatic := true
@export var magazine := 24
@export var reload_time := 1.6
@export var max_range := 150.0
## Rays per shot, spread over a cone `spread_degrees` wide.
@export var pellets := 1
@export var spread_degrees := 0.0
