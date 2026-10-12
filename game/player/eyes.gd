extends Node3D
## A cheese character's eyes, on the player's head so they turn and tilt with
## where the player looks. Measured as BodyShape's, which the player scales
## this node by.


func _init() -> void:
	var white := StandardMaterial3D.new()
	white.albedo_color = Color(1, 1, 1)
	var black := StandardMaterial3D.new()
	black.albedo_color = Color(0.05, 0.05, 0.08)
	for side in [-1.0, 1.0]:
		var eye := SphereMesh.new()
		eye.radius = BodyShape.EYE_RADIUS
		eye.height = BodyShape.EYE_RADIUS * 2.0
		eye.material = white
		var ball := MeshInstance3D.new()
		ball.mesh = eye
		ball.position = Vector3(BodyShape.EYE_APART * side, 0.0, 0.0)
		add_child(ball)
		var pupil := SphereMesh.new()
		pupil.radius = BodyShape.PUPIL_RADIUS
		pupil.height = BodyShape.PUPIL_RADIUS * 2.0
		pupil.material = black
		var dot := MeshInstance3D.new()
		dot.mesh = pupil
		dot.position = Vector3(BodyShape.EYE_APART * side, 0.0, -BodyShape.PUPIL_FORWARD)
		add_child(dot)
