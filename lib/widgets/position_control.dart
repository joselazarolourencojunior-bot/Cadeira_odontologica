import 'package:flutter/material.dart';
import 'control_button.dart';

class PositionControl extends StatelessWidget {
  final String title;
  final String upLabel;
  final String downLabel;
  final IconData upIcon;
  final IconData downIcon;
  final VoidCallback onUpPressed;
  final VoidCallback onDownPressed;

  const PositionControl({
    super.key,
    required this.title,
    required this.upLabel,
    required this.downLabel,
    required this.upIcon,
    required this.downIcon,
    required this.onUpPressed,
    required this.onDownPressed,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              title,
              style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 10),
            Row(
              children: [
                Expanded(
                  child: ControlButton(
                    label: upLabel,
                    icon: upIcon,
                    onPressed: onUpPressed,
                    color: Colors.green.shade600,
                    height: 60,
                  ),
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: ControlButton(
                    label: downLabel,
                    icon: downIcon,
                    onPressed: onDownPressed,
                    color: Colors.red.shade600,
                    height: 60,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
