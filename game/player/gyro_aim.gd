class_name GyroAim
## Aiming by turning a phone, or a controller that can tell: how fast the
## device turns becomes how fast the view does, for fine aim on top of the
## thumbs.
##
## The device is held sideways, so turning it left and right is turning it
## about what is, held upright, its own across axis, and tilting it is
## turning it about its upright one.

const SETTING := "gyro_aim"
const SENSITIVITY := "gyro_sensitivity"
## Turning slower than this, in radians a second, is a hand's tremble or the
## sensor's drift, and is ignored.
const STILL := 0.03


## How far, in radians across and up (as _look takes them), the view turns
## in `delta` seconds for a device turning at `rate` (radians a second about
## each of its own axes), held sideways the other way round if `flipped`,
## at `sensitivity` times the usual.
static func look_from(rate: Vector3, flipped: bool, sensitivity: float, delta: float) -> Vector2:
	var across := rate.x if not flipped else -rate.x
	var up := rate.y if not flipped else -rate.y
	var turned := Vector2(-across, -up)
	if turned.length() < STILL:
		return Vector2.ZERO
	return turned * sensitivity * delta
