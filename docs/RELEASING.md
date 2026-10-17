# 업데이트 배포

NestView는 Sparkle 2를 사용합니다. 새 버전 알림에서 사용자가 설치를 선택하면 서명을 검증하고 앱을 교체한 뒤 재실행합니다. 기본 확인 주기는 하루이며, 자동 다운로드·무인 설치는 하지 않습니다.

## 저장소 연결

기본 업데이트 저장소는 `RilakKuma2/NestView`입니다. 포크를 배포할 때만 `NESTVIEW_UPDATE_REPOSITORY=소유자/저장소`를 변경하세요.

```sh
./scripts/build-macos.sh
```

원본 qView 저장소는 업데이트 서버로 사용하지 않습니다.

## 새 릴리즈

1. `CMakeLists.txt`의 프로젝트 버전과 `PHOTO_BUILD_NUMBER`를 올립니다.
2. 최초 서명 키가 저장된 Mac에서 아래 명령을 실행합니다.

```sh
python3 scripts/prepare-release.py
```

3. 소스를 커밋·푸시하고 해당 커밋으로 같은 버전의 GitHub Release를 만듭니다. 예: `v0.1`.
4. `release/updates/v0.1/`의 DMG, 앱 ZIP과 `appcast.xml`을 **모두** 첨부하고 일반 릴리즈를 Latest로 게시합니다. 자동 게시하지는 않습니다.
5. 별도 앱 사본으로 이전 버전에서 설치·재실행을 검증하세요.

ZIP만 올리거나 코드만 푸시해서는 업데이트되지 않습니다. 생성된 XML과 ZIP은 수정하면 서명이 무효화됩니다.

## 서명과 호환성

- 업데이트 Ed25519 개인 키는 macOS 키체인의 `org.nestview.viewer` 계정에 보관합니다. 공개 키만 소스에 포함됩니다. 키를 Git이나 릴리즈에 올리지 마세요.
- 번들 식별자는 `org.nestview.viewer`입니다. 이전 이름으로 설치한 앱과 설정은 자동 이전하지 않으며, 새 앱을 한 번 수동 설치하고 파일 연결을 다시 지정해야 합니다.
- 현재 패키지는 Apple Silicon·macOS 15 이상용이며 ad-hoc 서명입니다. 업데이트 서명과 Apple Developer ID 서명·공증은 별개입니다.
- 원본 qView와 포함된 서드파티 구성 요소의 라이선스 및 소스를 배포 시 함께 보존하세요.
