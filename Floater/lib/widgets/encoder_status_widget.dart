import 'package:flutter/material.dart';
import '../models/chair_state.dart';

/// Widget para exibir status dos encoders virtuais e limites
class EncoderStatusWidget extends StatelessWidget {
  final ChairState chairState;

  const EncoderStatusWidget({super.key, required this.chairState});

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 2,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Título
            Row(
              children: [
                Icon(Icons.linear_scale, color: Colors.blue.shade700, size: 20),
                const SizedBox(width: 8),
                const Text(
                  'Posições da Cadeira',
                  style: TextStyle(fontSize: 14, fontWeight: FontWeight.bold),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Icon(
                  chairState.gavetaOpen
                      ? Icons.inventory_2
                      : Icons.inventory_2_outlined,
                  size: 18,
                  color: chairState.gavetaOpen ? Colors.red : Colors.green,
                ),
                const SizedBox(width: 8),
                Text(
                  chairState.gavetaOpen ? 'Gaveta aberta' : 'Gaveta fechada',
                  style: TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                    color: chairState.gavetaOpen ? Colors.red : Colors.green,
                  ),
                ),
                const Spacer(),
                if (chairState.gavetaLockIgnored)
                  Text(
                    'IGNORADA',
                    style: TextStyle(
                      fontSize: 11,
                      fontWeight: FontWeight.w800,
                      color: Colors.orange.shade700,
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 16),

            // Indicadores de posição
            _buildPositionIndicator(
              label: 'Encosto',
              position: chairState.backPosition,
              maxPosition: 50,
              upLimit: chairState.backUpLimit,
              downLimit: chairState.backDownLimit,
              color: Colors.blue,
            ),
            const SizedBox(height: 12),

            _buildPositionIndicator(
              label: 'Assento',
              position: chairState.seatPosition,
              maxPosition: 50,
              upLimit: chairState.seatUpLimit,
              downLimit: chairState.seatDownLimit,
              color: Colors.green,
            ),
            const SizedBox(height: 12),

            _buildPositionIndicator(
              label: 'Perneira',
              position: chairState.legPosition,
              maxPosition: 50,
              upLimit: chairState.legUpLimit,
              downLimit: chairState.legDownLimit,
              color: Colors.orange,
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPositionIndicator({
    required String label,
    required int position,
    required int maxPosition,
    required bool upLimit,
    required bool downLimit,
    required Color color,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Label e valor
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(
              label,
              style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w600),
            ),
            Row(
              children: [
                // Indicador de limite inferior
                if (downLimit)
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 6,
                      vertical: 2,
                    ),
                    margin: const EdgeInsets.only(right: 4),
                    decoration: BoxDecoration(
                      color: Colors.orange.shade100,
                      borderRadius: BorderRadius.circular(4),
                      border: Border.all(color: Colors.orange.shade400),
                    ),
                    child: Row(
                      children: [
                        Icon(
                          Icons.arrow_downward,
                          size: 10,
                          color: Colors.orange.shade700,
                        ),
                        const SizedBox(width: 2),
                        Text(
                          'Limite',
                          style: TextStyle(
                            fontSize: 8,
                            fontWeight: FontWeight.bold,
                            color: Colors.orange.shade700,
                          ),
                        ),
                      ],
                    ),
                  ),

                // Indicador de limite superior
                if (upLimit)
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 6,
                      vertical: 2,
                    ),
                    margin: const EdgeInsets.only(right: 4),
                    decoration: BoxDecoration(
                      color: Colors.orange.shade100,
                      borderRadius: BorderRadius.circular(4),
                      border: Border.all(color: Colors.orange.shade400),
                    ),
                    child: Row(
                      children: [
                        Icon(
                          Icons.arrow_upward,
                          size: 10,
                          color: Colors.orange.shade700,
                        ),
                        const SizedBox(width: 2),
                        Text(
                          'Limite',
                          style: TextStyle(
                            fontSize: 8,
                            fontWeight: FontWeight.bold,
                            color: Colors.orange.shade700,
                          ),
                        ),
                      ],
                    ),
                  ),

                // Valor da posição
                Text(
                  '$position/$maxPosition',
                  style: TextStyle(
                    fontSize: 11,
                    fontWeight: FontWeight.bold,
                    color: color,
                  ),
                ),
              ],
            ),
          ],
        ),
        const SizedBox(height: 6),

        // Barra de progresso
        Stack(
          children: [
            // Background
            Container(
              height: 8,
              decoration: BoxDecoration(
                color: Colors.grey.shade200,
                borderRadius: BorderRadius.circular(4),
              ),
            ),

            // Progress
            FractionallySizedBox(
              widthFactor: position / maxPosition,
              child: Container(
                height: 8,
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: [color.withValues(alpha: 0.6), color],
                  ),
                  borderRadius: BorderRadius.circular(4),
                  boxShadow: [
                    BoxShadow(
                      color: color.withValues(alpha: 0.3),
                      blurRadius: 4,
                      offset: const Offset(0, 2),
                    ),
                  ],
                ),
              ),
            ),

            // Marcador de limite inferior (0)
            if (downLimit && position == 0)
              Positioned(
                left: 0,
                child: Container(
                  width: 3,
                  height: 8,
                  decoration: BoxDecoration(
                    color: Colors.orange.shade700,
                    borderRadius: const BorderRadius.only(
                      topLeft: Radius.circular(4),
                      bottomLeft: Radius.circular(4),
                    ),
                  ),
                ),
              ),

            // Marcador de limite superior (50)
            if (upLimit && position == maxPosition)
              Positioned(
                right: 0,
                child: Container(
                  width: 3,
                  height: 8,
                  decoration: BoxDecoration(
                    color: Colors.orange.shade700,
                    borderRadius: const BorderRadius.only(
                      topRight: Radius.circular(4),
                      bottomRight: Radius.circular(4),
                    ),
                  ),
                ),
              ),
          ],
        ),
      ],
    );
  }
}
