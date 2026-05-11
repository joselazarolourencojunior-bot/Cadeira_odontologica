import 'package:flutter/material.dart';
import 'package:vector_math/vector_math_64.dart' as vector_math;
import '../models/chair_state.dart';

class ChairVisualization extends StatelessWidget {
  final ChairState chairState;

  const ChairVisualization({super.key, required this.chairState});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 380,
      height: 550,
      clipBehavior: Clip.hardEdge,
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: [Colors.grey.shade50, Colors.grey.shade100],
        ),
        borderRadius: BorderRadius.circular(25),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.08),
            blurRadius: 30,
            offset: const Offset(0, 15),
          ),
        ],
      ),
      child: Stack(
        children: [
          // Sombra da cadeira no chão
          Positioned(
            bottom: 40,
            left: 50,
            right: 50,
            child: Container(
              height: 25,
              decoration: BoxDecoration(
                gradient: RadialGradient(
                  colors: [
                    Colors.black.withValues(alpha: 0.25),
                    Colors.black.withValues(alpha: 0.1),
                    Colors.transparent,
                  ],
                ),
                borderRadius: BorderRadius.circular(100),
              ),
            ),
          ),

          // Refletor 3D
          if (chairState.reflectorOn)
            Positioned(
              top: 30,
              left: 0,
              right: 0,
              child: Center(
                child: AnimatedContainer(
                  duration: const Duration(milliseconds: 300),
                  child: Column(
                    children: [
                      // Braço do refletor
                      Transform(
                        transform: Matrix4.identity()
                          ..setEntry(3, 2, 0.001)
                          ..rotateX(-0.3),
                        alignment: Alignment.bottomCenter,
                        child: Container(
                          width: 10,
                          height: 70,
                          decoration: BoxDecoration(
                            gradient: LinearGradient(
                              begin: Alignment.topCenter,
                              end: Alignment.bottomCenter,
                              colors: [
                                Colors.grey.shade700,
                                Colors.grey.shade500,
                                Colors.grey.shade600,
                              ],
                            ),
                            borderRadius: BorderRadius.circular(5),
                            boxShadow: [
                              BoxShadow(
                                color: Colors.black.withValues(alpha: 0.4),
                                blurRadius: 6,
                                offset: const Offset(2, 3),
                              ),
                            ],
                          ),
                        ),
                      ),
                      // Lâmpada do refletor
                      Container(
                        width: 60,
                        height: 60,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          gradient: RadialGradient(
                            colors: [
                              Colors.yellow.shade200,
                              Colors.yellow.shade400,
                              Colors.yellow.shade700,
                              Colors.orange.shade800,
                            ],
                          ),
                          boxShadow: [
                            BoxShadow(
                              color: Colors.yellow.withValues(alpha: 0.7),
                              blurRadius: 35,
                              spreadRadius: 12,
                            ),
                            BoxShadow(
                              color: Colors.orange.withValues(alpha: 0.5),
                              blurRadius: 55,
                              spreadRadius: 25,
                            ),
                            BoxShadow(
                              color: Colors.white.withValues(alpha: 0.3),
                              blurRadius: 20,
                              spreadRadius: 5,
                            ),
                          ],
                        ),
                        child: Center(
                          child: Container(
                            width: 40,
                            height: 40,
                            decoration: BoxDecoration(
                              shape: BoxShape.circle,
                              gradient: RadialGradient(
                                colors: [Colors.white, Colors.yellow.shade100],
                              ),
                            ),
                            child: const Icon(
                              Icons.wb_sunny,
                              color: Colors.white,
                              size: 28,
                            ),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),

          // Cadeira 3D
          Center(
            child: Transform(
              transform: Matrix4.identity()
                ..setEntry(3, 2, 0.002)
                ..rotateX(0.3)
                ..rotateY(-0.15),
              alignment: Alignment.center,
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const SizedBox(height: 60),

                  // Encosto 3D
                  AnimatedContainer(
                    duration: const Duration(milliseconds: 600),
                    curve: Curves.easeInOut,
                    transform: Matrix4.identity()
                      ..setEntry(3, 2, 0.002)
                      ..rotateX(_getBackRotation()),
                    transformAlignment: Alignment.bottomCenter,
                    child: Container(
                      width: 135,
                      height: 140,
                      decoration: BoxDecoration(
                        gradient: LinearGradient(
                          begin: Alignment.topLeft,
                          end: Alignment.bottomRight,
                          colors: [
                            _getBackColor().withValues(alpha: 0.95),
                            _getBackColor(),
                            _getBackColor().withValues(alpha: 0.8),
                            _getBackColor().withValues(alpha: 0.75),
                          ],
                        ),
                        borderRadius: const BorderRadius.only(
                          topLeft: Radius.circular(35),
                          topRight: Radius.circular(35),
                          bottomLeft: Radius.circular(18),
                          bottomRight: Radius.circular(18),
                        ),
                        boxShadow: [
                          BoxShadow(
                            color: Colors.black.withValues(alpha: 0.35),
                            blurRadius: 25,
                            offset: const Offset(0, 12),
                          ),
                          BoxShadow(
                            color: _getBackColor().withValues(alpha: 0.4),
                            blurRadius: 12,
                            offset: const Offset(0, 6),
                          ),
                        ],
                      ),
                      child: Stack(
                        children: [
                          // Textura do estofado
                          Positioned.fill(
                            child: Container(
                              decoration: BoxDecoration(
                                borderRadius: const BorderRadius.only(
                                  topLeft: Radius.circular(35),
                                  topRight: Radius.circular(35),
                                  bottomLeft: Radius.circular(18),
                                  bottomRight: Radius.circular(18),
                                ),
                                border: Border.all(
                                  color: Colors.white.withValues(alpha: 0.35),
                                  width: 2.5,
                                ),
                              ),
                            ),
                          ),
                          // Brilho superior realista
                          Positioned(
                            top: 8,
                            left: 15,
                            right: 15,
                            child: Container(
                              height: 50,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  begin: Alignment.topCenter,
                                  end: Alignment.bottomCenter,
                                  colors: [
                                    Colors.white.withValues(alpha: 0.4),
                                    Colors.white.withValues(alpha: 0.2),
                                    Colors.white.withValues(alpha: 0.05),
                                    Colors.transparent,
                                  ],
                                ),
                                borderRadius: BorderRadius.circular(30),
                              ),
                            ),
                          ),
                          // Sombra interna lateral esquerda
                          Positioned(
                            left: 0,
                            top: 0,
                            bottom: 0,
                            child: Container(
                              width: 15,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  begin: Alignment.centerLeft,
                                  end: Alignment.centerRight,
                                  colors: [
                                    Colors.black.withValues(alpha: 0.2),
                                    Colors.transparent,
                                  ],
                                ),
                              ),
                            ),
                          ),
                          // Sombra interna lateral direita
                          Positioned(
                            right: 0,
                            top: 0,
                            bottom: 0,
                            child: Container(
                              width: 15,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  begin: Alignment.centerRight,
                                  end: Alignment.centerLeft,
                                  colors: [
                                    Colors.black.withValues(alpha: 0.2),
                                    Colors.transparent,
                                  ],
                                ),
                              ),
                            ),
                          ),
                          // Costuras decorativas verticais
                          Positioned(
                            left: 35,
                            top: 15,
                            bottom: 15,
                            child: Container(
                              width: 2,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  colors: [
                                    Colors.transparent,
                                    Colors.black.withValues(alpha: 0.3),
                                    Colors.black.withValues(alpha: 0.3),
                                    Colors.transparent,
                                  ],
                                ),
                                borderRadius: BorderRadius.circular(1),
                              ),
                            ),
                          ),
                          Positioned(
                            right: 35,
                            top: 15,
                            bottom: 15,
                            child: Container(
                              width: 2,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  colors: [
                                    Colors.transparent,
                                    Colors.black.withValues(alpha: 0.3),
                                    Colors.black.withValues(alpha: 0.3),
                                    Colors.transparent,
                                  ],
                                ),
                                borderRadius: BorderRadius.circular(1),
                              ),
                            ),
                          ),
                          // Costuras decorativas horizontais
                          Center(
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                              children: [
                                _buildStitchLine(),
                                _buildStitchLine(),
                                _buildStitchLine(),
                                _buildStitchLine(),
                              ],
                            ),
                          ),
                          // Botões capitonê (efeito estofado)
                          ..._buildCapitoneButtons(),
                        ],
                      ),
                    ),
                  ),

                  const SizedBox(height: 5),

                  // Braços/Apoios laterais
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      _buildArmrest(),
                      const SizedBox(width: 145),
                      _buildArmrest(),
                    ],
                  ),

                  const SizedBox(height: 5),

                  // Assento 3D
                  AnimatedContainer(
                    duration: const Duration(milliseconds: 600),
                    curve: Curves.easeInOut,
                    transform: Matrix4.identity()
                      ..setEntry(3, 2, 0.002)
                      ..translateByVector3(
                        vector_math.Vector3(
                          0.0,
                          _getSeatVerticalPosition(),
                          0.0,
                        ),
                      ),
                    child: Container(
                      width: 150,
                      height: 80,
                      decoration: BoxDecoration(
                        gradient: LinearGradient(
                          begin: Alignment.topCenter,
                          end: Alignment.bottomCenter,
                          colors: [
                            _getSeatColor().withValues(alpha: 0.95),
                            _getSeatColor(),
                            _getSeatColor().withValues(alpha: 0.75),
                            _getSeatColor().withValues(alpha: 0.65),
                          ],
                        ),
                        borderRadius: BorderRadius.circular(22),
                        boxShadow: [
                          BoxShadow(
                            color: Colors.black.withValues(alpha: 0.45),
                            blurRadius: 28,
                            offset: const Offset(0, 16),
                          ),
                          BoxShadow(
                            color: _getSeatColor().withValues(alpha: 0.45),
                            blurRadius: 12,
                            offset: const Offset(0, 6),
                          ),
                        ],
                      ),
                      child: Stack(
                        children: [
                          // Divisão central do assento
                          Center(
                            child: Container(
                              width: 5,
                              height: 65,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  colors: [
                                    Colors.black.withValues(alpha: 0.4),
                                    Colors.black.withValues(alpha: 0.2),
                                    Colors.black.withValues(alpha: 0.1),
                                  ],
                                ),
                                borderRadius: BorderRadius.circular(2.5),
                                boxShadow: [
                                  BoxShadow(
                                    color: Colors.black.withValues(alpha: 0.3),
                                    blurRadius: 3,
                                    offset: const Offset(1, 1),
                                  ),
                                ],
                              ),
                            ),
                          ),
                          // Brilho superior realista
                          Positioned(
                            top: 6,
                            left: 12,
                            right: 12,
                            child: Container(
                              height: 30,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  begin: Alignment.topCenter,
                                  end: Alignment.bottomCenter,
                                  colors: [
                                    Colors.white.withValues(alpha: 0.4),
                                    Colors.white.withValues(alpha: 0.2),
                                    Colors.white.withValues(alpha: 0.05),
                                    Colors.transparent,
                                  ],
                                ),
                                borderRadius: BorderRadius.circular(20),
                              ),
                            ),
                          ),
                          // Sombra frontal
                          Positioned(
                            top: 0,
                            left: 0,
                            right: 0,
                            child: Container(
                              height: 12,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  begin: Alignment.topCenter,
                                  end: Alignment.bottomCenter,
                                  colors: [
                                    Colors.black.withValues(alpha: 0.15),
                                    Colors.transparent,
                                  ],
                                ),
                              ),
                            ),
                          ),
                          // Sombra traseira
                          Positioned(
                            bottom: 0,
                            left: 0,
                            right: 0,
                            child: Container(
                              height: 12,
                              decoration: BoxDecoration(
                                gradient: LinearGradient(
                                  begin: Alignment.bottomCenter,
                                  end: Alignment.topCenter,
                                  colors: [
                                    Colors.black.withValues(alpha: 0.25),
                                    Colors.transparent,
                                  ],
                                ),
                              ),
                            ),
                          ),
                          // Bordas 3D
                          Positioned.fill(
                            child: Container(
                              decoration: BoxDecoration(
                                borderRadius: BorderRadius.circular(22),
                                border: Border.all(
                                  color: Colors.white.withValues(alpha: 0.35),
                                  width: 2.5,
                                ),
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),

                  const SizedBox(height: 8),

                  // Base/Coluna Central 3D com Hidráulica
                  Column(
                    children: [
                      // Parte superior da coluna (hidráulica)
                      Container(
                        width: 28,
                        height: 50,
                        decoration: BoxDecoration(
                          gradient: LinearGradient(
                            begin: Alignment.centerLeft,
                            end: Alignment.centerRight,
                            colors: [
                              Colors.grey.shade700,
                              Colors.grey.shade400,
                              Colors.grey.shade300,
                              Colors.grey.shade400,
                              Colors.grey.shade700,
                            ],
                          ),
                          borderRadius: BorderRadius.circular(17),
                          boxShadow: [
                            BoxShadow(
                              color: Colors.black.withValues(alpha: 0.4),
                              blurRadius: 12,
                              offset: const Offset(5, 6),
                            ),
                            BoxShadow(
                              color: Colors.white.withValues(alpha: 0.3),
                              blurRadius: 5,
                              offset: const Offset(-2, -2),
                            ),
                          ],
                        ),
                        child: Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                            children: [
                              // Anéis metálicos da hidráulica
                              Container(
                                width: 26,
                                height: 3,
                                decoration: BoxDecoration(
                                  color: Colors.grey.shade600,
                                  borderRadius: BorderRadius.circular(2),
                                  boxShadow: [
                                    BoxShadow(
                                      color: Colors.black.withValues(
                                        alpha: 0.3,
                                      ),
                                      blurRadius: 2,
                                      offset: const Offset(0, 1),
                                    ),
                                  ],
                                ),
                              ),
                              Container(
                                width: 26,
                                height: 3,
                                decoration: BoxDecoration(
                                  color: Colors.grey.shade600,
                                  borderRadius: BorderRadius.circular(2),
                                  boxShadow: [
                                    BoxShadow(
                                      color: Colors.black.withValues(
                                        alpha: 0.3,
                                      ),
                                      blurRadius: 2,
                                      offset: const Offset(0, 1),
                                    ),
                                  ],
                                ),
                              ),
                              Container(
                                width: 26,
                                height: 3,
                                decoration: BoxDecoration(
                                  color: Colors.grey.shade600,
                                  borderRadius: BorderRadius.circular(2),
                                  boxShadow: [
                                    BoxShadow(
                                      color: Colors.black.withValues(
                                        alpha: 0.3,
                                      ),
                                      blurRadius: 2,
                                      offset: const Offset(0, 1),
                                    ),
                                  ],
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                      // Base com rodízios
                      const SizedBox(height: 4),
                      Stack(
                        alignment: Alignment.center,
                        children: [
                          // Base estrela (5 pontas)
                          ...List.generate(5, (index) {
                            final angle = (index * 72) * 3.14159 / 180;
                            return Transform.rotate(
                              angle: angle,
                              child: Container(
                                width: 65,
                                height: 11,
                                decoration: BoxDecoration(
                                  gradient: LinearGradient(
                                    begin: Alignment.centerLeft,
                                    end: Alignment.centerRight,
                                    colors: [
                                      Colors.grey.shade800,
                                      Colors.grey.shade600,
                                      Colors.grey.shade500,
                                      Colors.grey.shade600,
                                      Colors.grey.shade800,
                                    ],
                                  ),
                                  borderRadius: BorderRadius.circular(6),
                                  boxShadow: [
                                    BoxShadow(
                                      color: Colors.black.withValues(
                                        alpha: 0.3,
                                      ),
                                      blurRadius: 4,
                                      offset: const Offset(0, 2),
                                    ),
                                  ],
                                ),
                              ),
                            );
                          }),
                          // Centro da base
                          Container(
                            width: 25,
                            height: 25,
                            decoration: BoxDecoration(
                              shape: BoxShape.circle,
                              gradient: RadialGradient(
                                colors: [
                                  Colors.grey.shade400,
                                  Colors.grey.shade600,
                                  Colors.grey.shade700,
                                ],
                              ),
                              boxShadow: [
                                BoxShadow(
                                  color: Colors.black.withValues(alpha: 0.4),
                                  blurRadius: 6,
                                  offset: const Offset(0, 3),
                                ),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),

                  const SizedBox(height: 6),

                  // Apoio de Pernas 3D (duas pernas)
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      _buildLegSupport(),
                      const SizedBox(width: 28),
                      _buildLegSupport(),
                    ],
                  ),
                ],
              ),
            ),
          ),

          // Legendas de status
          Positioned(
            bottom: 10,
            left: 0,
            right: 0,
            child: Center(
              child: Wrap(
                spacing: 4,
                runSpacing: 4,
                alignment: WrapAlignment.center,
                children: [
                  if (chairState.reflectorOn)
                    _buildStatusBadge('💡 Refletor', Colors.yellow.shade700),
                  if (chairState.backUpOn)
                    _buildStatusBadge('↑ Sentar', Colors.blue.shade600),
                  if (chairState.backDownOn)
                    _buildStatusBadge('↓ Deitar', Colors.orange.shade600),
                  if (chairState.seatUpOn)
                    _buildStatusBadge('⬆ Subir', Colors.blue.shade600),
                  if (chairState.seatDownOn)
                    _buildStatusBadge('⬇ Descer', Colors.orange.shade600),
                  if (chairState.upperLegsOn)
                    _buildStatusBadge('⬆ Pernas', Colors.green.shade600),
                  if (chairState.lowerLegsOn)
                    _buildStatusBadge('⬇ Pernas', Colors.red.shade600),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildLegSupport() {
    return AnimatedContainer(
      duration: const Duration(milliseconds: 600),
      curve: Curves.easeInOut,
      transform: Matrix4.identity()
        ..setEntry(3, 2, 0.002)
        ..rotateX(_getLegRotation()),
      transformAlignment: Alignment.topCenter,
      child: Container(
        width: 48,
        height: 88,
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              _getLegColor().withValues(alpha: 0.95),
              _getLegColor(),
              _getLegColor().withValues(alpha: 0.8),
              _getLegColor().withValues(alpha: 0.75),
            ],
          ),
          borderRadius: BorderRadius.circular(17),
          boxShadow: [
            BoxShadow(
              color: Colors.black.withValues(alpha: 0.35),
              blurRadius: 18,
              offset: const Offset(0, 11),
            ),
            BoxShadow(
              color: _getLegColor().withValues(alpha: 0.35),
              blurRadius: 10,
              offset: const Offset(0, 5),
            ),
          ],
        ),
        child: Stack(
          children: [
            // Brilho superior
            Positioned(
              top: 8,
              left: 8,
              right: 8,
              child: Container(
                height: 30,
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                    colors: [
                      Colors.white.withValues(alpha: 0.3),
                      Colors.white.withValues(alpha: 0.1),
                      Colors.transparent,
                    ],
                  ),
                  borderRadius: BorderRadius.circular(15),
                ),
              ),
            ),
            // Bordas 3D
            Positioned.fill(
              child: Container(
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(17),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.25),
                    width: 2,
                  ),
                ),
              ),
            ),
            // Linha central decorativa
            Center(
              child: Container(
                width: 3,
                height: 70,
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: [
                      Colors.black.withValues(alpha: 0.2),
                      Colors.black.withValues(alpha: 0.1),
                      Colors.transparent,
                    ],
                  ),
                  borderRadius: BorderRadius.circular(1.5),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStitchLine() {
    return Container(
      width: 110,
      height: 2.5,
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: [
            Colors.transparent,
            Colors.black.withValues(alpha: 0.25),
            Colors.black.withValues(alpha: 0.3),
            Colors.black.withValues(alpha: 0.25),
            Colors.transparent,
          ],
        ),
        borderRadius: BorderRadius.circular(1),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.15),
            blurRadius: 2,
            offset: const Offset(0, 1),
          ),
        ],
      ),
    );
  }

  // Botões capitonê para efeito estofado realista
  List<Widget> _buildCapitoneButtons() {
    return [
      Positioned(top: 35, left: 50, child: _buildCapitoneButton()),
      Positioned(top: 35, right: 50, child: _buildCapitoneButton()),
      Positioned(top: 75, left: 50, child: _buildCapitoneButton()),
      Positioned(top: 75, right: 50, child: _buildCapitoneButton()),
      Positioned(top: 115, left: 50, child: _buildCapitoneButton()),
      Positioned(top: 115, right: 50, child: _buildCapitoneButton()),
    ];
  }

  Widget _buildCapitoneButton() {
    return Container(
      width: 8,
      height: 8,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        gradient: RadialGradient(
          colors: [
            Colors.black.withValues(alpha: 0.4),
            Colors.black.withValues(alpha: 0.2),
            Colors.transparent,
          ],
        ),
      ),
      child: Center(
        child: Container(
          width: 4,
          height: 4,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: Colors.black.withValues(alpha: 0.5),
          ),
        ),
      ),
    );
  }

  // Apoio de braço lateral
  Widget _buildArmrest() {
    return Container(
      width: 15,
      height: 65,
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topCenter,
          end: Alignment.bottomCenter,
          colors: [
            Colors.grey.shade600,
            Colors.grey.shade500,
            Colors.grey.shade600,
          ],
        ),
        borderRadius: BorderRadius.circular(8),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.3),
            blurRadius: 6,
            offset: const Offset(2, 3),
          ),
        ],
      ),
    );
  }

  Widget _buildStatusBadge(String label, Color color) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [color, color.withValues(alpha: 0.85)],
        ),
        borderRadius: BorderRadius.circular(20),
        boxShadow: [
          BoxShadow(
            color: color.withValues(alpha: 0.45),
            blurRadius: 10,
            offset: const Offset(0, 3),
          ),
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.2),
            blurRadius: 5,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: Text(
        label,
        style: const TextStyle(
          color: Colors.white,
          fontSize: 11,
          fontWeight: FontWeight.bold,
          letterSpacing: 0.3,
        ),
      ),
    );
  }

  double _getBackRotation() {
    if (chairState.backDownOn) {
      return 0.8; // Mais reclinado
    } else if (chairState.backUpOn) {
      return 0.1; // Mais vertical
    }
    return 0.4; // Posição intermediária
  }

  double _getSeatVerticalPosition() {
    if (chairState.seatUpOn) {
      return -15.0; // Mais alto
    } else if (chairState.seatDownOn) {
      return 15.0; // Mais baixo
    }
    return 0.0; // Posição normal
  }

  double _getLegRotation() {
    if (chairState.upperLegsOn) {
      return -0.6; // Pernas elevadas
    } else if (chairState.lowerLegsOn) {
      return 0.4; // Pernas abaixadas
    }
    return 0.0; // Posição horizontal
  }

  Color _getBackColor() {
    if (chairState.backUpOn) {
      return Colors.blue.shade600;
    } else if (chairState.backDownOn) {
      return Colors.orange.shade600;
    }
    return Colors.brown.shade600;
  }

  Color _getSeatColor() {
    if (chairState.seatUpOn) {
      return Colors.blue.shade700;
    } else if (chairState.seatDownOn) {
      return Colors.orange.shade700;
    }
    return Colors.brown.shade700;
  }

  Color _getLegColor() {
    if (chairState.upperLegsOn) {
      return Colors.green.shade600;
    } else if (chairState.lowerLegsOn) {
      return Colors.red.shade600;
    }
    return Colors.brown.shade600;
  }
}
