let Genode =
        env:DHALL_GENODE
      ? https://git.sr.ht/~ehmry/dhall-genode/blob/v11.0.0/package.dhall sha256:4336da47b739fe6b9e117436404ca56af0cfec15805abb0baf6f5ba366c7e5ce

let Prelude = Genode.Prelude

let Configuration =
        { arch : < x86_32 | x86_64 >
        , config : Genode.Init.Type
        , rom : Prelude.Map.Type Text Text
        }
      : Type

in    λ ( boot
        : Configuration
        )
    → let NaturalIndex =
            { index : Natural, value : Text }

      let TextIndex = { index : Text, value : Text }

      let moduleKeys =
            let keys = Prelude.Map.keys Text Text boot.rom

            in  [ "config" ] # keys

      let moduleValues =
            let values = Prelude.Map.values Text Text boot.rom

            let incbin =
                  Prelude.List.map
                    Text
                    Text
                    (λ(path : Text) → ".incbin ${Text/show path}")
                    values

            in    [ ".ascii ${Text/show (Genode.Init.render boot.config)}" ]
                # incbin

      let map =
              λ(list : List Text)
            → λ(f : TextIndex → Text)
            → let indexedNatural = Prelude.List.indexed Text list

              let indexed =
                    Prelude.List.map
                      NaturalIndex
                      TextIndex
                      (   λ(x : NaturalIndex)
                        → { index = Prelude.Natural.show (x.index + 1)
                          , value = x.value
                          }
                      )
                      indexedNatural

              let texts = Prelude.List.map TextIndex Text f indexed

              in  Prelude.Text.concatSep "\n" texts

      let mapNames = map moduleKeys

      let mapValues = map moduleValues

      let addressType = merge { x86_32 = ".long", x86_64 = ".quad" } boot.arch

      in      ''
              .set MIN_PAGE_SIZE_LOG2, 12
              .set DATA_ACCESS_ALIGNM_LOG2, 3
              .section .data
              .p2align DATA_ACCESS_ALIGNM_LOG2
              .global _boot_modules_headers_begin
              _boot_modules_headers_begin:

              ''
          ++  mapNames
                (   λ ( m
                      : TextIndex
                      )
                  → ''
                    ${addressType} _boot_module_${m.index}_name
                    ${addressType} _boot_module_${m.index}_begin
                    ${addressType} _boot_module_${m.index}_end - _boot_module_${m.index}_begin
                    ''
                )
          ++  ''
              .global _boot_modules_headers_end
              _boot_modules_headers_end:

              ''
          ++  mapNames
                (   λ(m : TextIndex)
                  → ''
                    .p2align DATA_ACCESS_ALIGNM_LOG2
                    _boot_module_${m.index}_name:
                    .string "${m.value}"
                    .byte 0
                    ''
                )
          ++  ''
              .section .data.boot_modules_binaries

              .global _boot_modules_binaries_begin
              _boot_modules_binaries_begin:

              ''
          ++  mapValues
                (   λ(m : TextIndex)
                  → ''
                    .p2align MIN_PAGE_SIZE_LOG2
                    _boot_module_${m.index}_begin:
                    ${m.value}
                    _boot_module_${m.index}_end:
                    ''
                )
          ++  ''
              .p2align MIN_PAGE_SIZE_LOG2
              .global _boot_modules_binaries_end
              _boot_modules_binaries_end:
              ''
