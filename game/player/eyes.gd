extends Node3D
## A cheese character's eyes, on the player's head so they turn and tilt with
## where the player looks.


func _init() -> void:
	var white := StandardMaterial3D.new()
	white.albedo_color = Color(1, 1, 1)
	var black := StandardMaterial3D.new()
	black.albedo_color = Color(0.05, 0.05, 0.08)
	for side in [-1.0, 1.0]:
		var eye := SphereMesh.new()
		eye.radius = 0.075
		eye.height = 0.15
		eye.material = white
		var ball := MeshInstance3D.new()
		ball.mesh = eye
		ball.position = Vector3(0.1 * side, 0.0, 0.0)
		add_child(ball)
		var pupil := SphereMesh.new()
		pupil.radius = 0.035
		pupil.height = 0.07
		pupil.material = black
		var dot := MeshInstance3D.new()
		dot.mesh = pupil
		dot.position = Vector3(0.1 * side, 0.0, -0.055)
		add_child(dot)
