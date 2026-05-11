import 'package:flutter/material.dart';

class ControlButton extends StatelessWidget {
  final String label;
  final IconData icon;
  final VoidCallback? onPressed;
  final VoidCallback? onLongPressStart;
  final VoidCallback? onLongPressEnd;
  final Color color;
  final double height;
  final bool isActive;

  const ControlButton({
    super.key,
    required this.label,
    required this.icon,
    this.onPressed,
    this.onLongPressStart,
    this.onLongPressEnd,
    this.color = Colors.blue,
    this.height = 80,
    this.isActive = false,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: height,
      child: GestureDetector(
        onTap: onPressed,
        onLongPressStart: onLongPressStart != null ? (_) => onLongPressStart!() : null,
        onLongPressEnd: onLongPressEnd != null ? (_) => onLongPressEnd!() : null,
        child: ElevatedButton(
          onPressed: null, // Disable default onPressed since we use GestureDetector
          style: ElevatedButton.styleFrom(
            backgroundColor: isActive
                ? color.withValues(alpha: 1.0)
                : color.withValues(alpha: 0.6),
            foregroundColor: Colors.white,
            elevation: isActive ? 8 : 4,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
              side: isActive
                  ? BorderSide(color: Colors.white, width: 2)
                  : BorderSide.none,
            ),
          ),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, size: 24),
              const SizedBox(height: 4),
              Text(
                label,
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 12, fontWeight: FontWeight.bold),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
