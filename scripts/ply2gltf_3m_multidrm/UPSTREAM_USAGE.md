# MultiDRM PLY to glTF 변환기 사용 가이드

## Windows에서 빌드하기

### 요구사항
- Visual Studio 2019 이상 (C++17 지원)
- CMake 3.16 이상
- Git

### 빌드 방법

```powershell
# 1. 프로젝트 디렉토리로 이동
cd mpeg-ply2gltf_multiDRM

# 2. 빌드 디렉토리 생성
mkdir build
cd build

# 3. CMake 설정 (Visual Studio용)
cmake .. -G "Visual Studio 16 2019" -A x64

# 또는 Visual Studio 2022인 경우
cmake .. -G "Visual Studio 17 2022" -A x64

# 4. 빌드 실행
cmake --build . --config Release

# 실행 파일 위치: build\bin\Release\ply2gltf.exe
```

## 사용 방법

### 기본 변환 (DRM 없음)
```bash
ply2gltf.exe input.ply output.glb
```

### 단일 DRM (SIMPLE_XOR)
```bash
ply2gltf.exe input.ply output.glb --drm \
  --drm-system xor \
  --drm-key-id "0123456789abcdef0123456789abcdef" \
  --drm-key "mykey123" \
  --drm-encrypted-attributes "sh,scale"
```

### MultiDRM (Widevine + PlayReady)
```bash
ply2gltf.exe input.ply output.glb --drm \
  --drm-system widevine \
  --drm-key-id "0123456789abcdef0123456789abcdef" \
  --drm-key "widevine_key" \
  --drm-system playready \
  --drm-key-id "fedcba9876543210fedcba9876543210" \
  --drm-key "playready_key" \
  --drm-encrypted-attributes "sh,scale" \
  --verbose
```

### 옵션 설명

#### 기본 옵션
- `-h, --help`: 도움말 표시
- `-o, --output <file>`: 출력 파일 경로
- `-f, --format <glb|gltf>`: 출력 형식 (기본값: glb)
- `-v, --verbose`: 상세 출력 활성화
- `-p, --progressive`: 점진적 로딩을 위한 데이터 구성

#### DRM 옵션
- `--drm`: DRM 패키징 활성화
- `--drm-encrypted-attributes <list>`: 암호화할 속성 목록 (쉼표로 구분)
  - 예: `"sh,scale"`, `"position,color"`, `"sh"` 등
  - 지원 속성: `position`, `color`, `orientation`, `scale`, `sh`

#### MultiDRM 옵션
- `--drm-system <system>`: DRM 시스템 지정 (`xor`, `widevine`, `playready`)
- `--drm-key-id <hex>`: Key ID (16바이트 hex 문자열)
- `--drm-key <key>`: 암호화 키
- `--drm-scheme-uri <uri>`: 커스텀 scheme URI (선택사항)
- `--drm-pssh <base64>`: PSSH box 데이터 (base64 인코딩, 선택사항)
- `--drm-license-url <url>`: 라이선스 서버 URL (선택사항)

### 속성 암호화 예시

```bash
# SH 계수만 암호화
ply2gltf.exe input.ply output.glb --drm \
  --drm-system widevine \
  --drm-key-id "0123456789abcdef0123456789abcdef" \
  --drm-key "key123" \
  --drm-encrypted-attributes "sh"

# Scale과 SH 암호화
ply2gltf.exe input.ply output.glb --drm \
  --drm-system widevine \
  --drm-key-id "0123456789abcdef0123456789abcdef" \
  --drm-key "key123" \
  --drm-encrypted-attributes "scale,sh"

# 모든 속성 암호화
ply2gltf.exe input.ply output.glb --drm \
  --drm-system widevine \
  --drm-key-id "0123456789abcdef0123456789abcdef" \
  --drm-key "key123" \
  --drm-encrypted-attributes "position,color,orientation,scale,sh"
```

## 출력 파일 구조

생성된 GLB 파일에는 다음 정보가 포함됩니다:

### EXT_content_protection 확장
```json
{
  "extensions": {
    "EXT_content_protection": {
      "systems": [
        {
          "schemeIdUri": "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd5121ed8",
          "keyId": "0123456789abcdef0123456789abcdef",
          "encryptedAccessors": [3, 4, 5, 6]
        },
        {
          "schemeIdUri": "urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95",
          "keyId": "fedcba9876543210fedcba9876543210",
          "encryptedAccessors": [3, 4, 5, 6]
        }
      ]
    }
  }
}
```

## Windows 렌더링 테스트

생성된 GLB 파일은 Windows 환경에서:
- Widevine: Chrome/Edge 브라우저에서 지원
- PlayReady: Windows 네이티브 DRM 지원

로 렌더링 테스트가 가능합니다.
