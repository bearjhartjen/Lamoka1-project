# Design Decisions Log

## 2026-08-25: Two-Stage Architecture Finalized

### Decision
Implement a two-stage converter: 10-14.6V → 48V boost → 200V isolated → H-bridge

### Rationale
- Single-stage direct boost to 200V was physically impossible with LM5122 duty limits
- 48V intermediate rail simplifies MOSFET voltage ratings
- Flyback isolation provides galvanic separation
- H-bridge with 200V bus achieves proper modulation index (M=0.85)

### Implementation Requirements

1. **48V Boost Stage**
   - LM5122 controller
   - Duty cycle: 70-79% (verified feasible)
   - Inductor: 10µH minimum
   - MOSFET: 100V, 15A

2. **Isolated Flyback Stage**  
   - Ns/Np = 4.2 turns ratio
   - Duty cycle: ~50%
   - EFD30 core minimum
   - Primary: 330V MOSFET
   - Secondary: 400V Schottky

3. **H-Bridge Stage**
   - 200V DC bus regulated
   - 300V MOSFETs required (200V + margin)
   - Gate drivers: isolated
   - Modulation: SPWM with M=0.85

## Component Selection Timeline

- [ ] Primary MOSFETs for boost stage
- [ ] Gate driver for boost stage  
- [ ] Transformer core and winding design
- [ ] Primary MOSFETs for isolated stage
- [ ] Secondary rectifiers
- [ ] Bulk capacitors (470µF 250V)
- [ ] Current sensing elements
- [ ] Protection components