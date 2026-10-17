# NestView

사진 탐색과 감상에 집중하는 macOS·Windows·Linux 이미지 뷰어입니다.

![NestView에서 이미지 정보와 필름스트립으로 사진 탐색](docs/images/nestview-preview.png)

## 주요 기능

- 폴더 갤러리와 필름스트립으로 빠르게 이미지 탐색
- 트랙패드 핀치 확대, 스크롤 이동, 마우스·키보드 조작
- 두 장 보기, 웹툰 연속 보기, 슬라이드쇼
- EXIF 정보, 북마크, PNG·JPEG·WebP 변환
- ZIP·CBZ·RAR·CBR·7z 등 압축파일 안의 이미지 열기
- 새 버전 확인과 다운로드, macOS의 서명된 앱 내 업데이트

## 설치

Apple Silicon Mac과 macOS 15 이상이 필요합니다. [최신 DMG](https://github.com/RilakKuma2/NestView/releases/latest/download/NestView-macOS-AppleSilicon.dmg)를 열고 **NestView.app**을 **응용 프로그램** 폴더로 옮겨 실행하세요.

Windows 10/11 64비트에서는 [Windows ZIP](https://github.com/RilakKuma2/NestView/releases/latest/download/NestView-Windows-x86_64.zip)을 풀고 **NestView.exe**를 실행합니다. 실행 파일과 함께 들어 있는 DLL·플러그인 폴더를 유지해야 합니다.

Ubuntu 24.04 이상(x86_64)에서는 다음 명령으로 설치합니다.

```sh
curl -fL -o NestView.deb https://github.com/RilakKuma2/NestView/releases/latest/download/NestView-Linux-x86_64.deb
sudo apt update
sudo apt install ./NestView.deb
nestview
```

## 사용

이미지·폴더·압축파일을 창에 끌어 놓아 열 수 있습니다.

| 작업 | 단축키 |
| --- | --- |
| 파일 열기 / 폴더 열기 | ⌘O / F |
| 이전 / 다음 이미지 | ← / → |
| 갤러리 | G |
| 원본 크기 / 창 맞춤 | 0 / 1 |
| 두 장 보기 / 웹툰 보기 | 7 / 5 |
| 이미지 정보 | Tab |
| 설정 | ⌘, |

Windows·Linux에서는 ⌘ 대신 **Ctrl**을 사용하며 설정은 **F5**로 열 수 있습니다. RAW는 LibRaw, EXIF는 Exiv2를 사용합니다. HEIC·AVIF·PSD 등은 OS별 이미지 플러그인에 따라 지원 범위가 다릅니다.

macOS 상단 **NestView → 업데이트 확인…**에서 새 버전을 확인합니다. 업데이트가 있을 때만 **자동 업데이트**와 **설치 파일 다운로드**를 선택할 수 있습니다. 최신 버전이면 최신 상태라는 안내를 표시합니다. 창의 파일 메뉴에서 백그라운드 업데이트 확인도 켜거나 끌 수 있습니다.

Windows·Linux에서는 메뉴의 **업데이트 확인…**으로 새 버전을 확인하고 해당 OS의 설치 파일을 다운로드합니다. 설치 파일이 아직 없는 경우에는 안내만 표시합니다. Windows·Linux의 앱 내 자동 설치는 지원하지 않습니다.

## 소스에서 빌드

Xcode Command Line Tools와 Homebrew가 필요합니다.

```sh
brew install qtbase qttools qtsvg qtimageformats libarchive cmake ninja
./scripts/build-macos.sh
./scripts/package-macos.sh
python3 scripts/package-dmg.py
```

빌드 결과는 `release/NestView.app`에 생성됩니다. 업데이트 배포 설정은 [릴리즈 안내](docs/RELEASING.md)를 참고하세요.

Windows·Linux의 빌드·패키징 방법은 [멀티플랫폼 빌드 안내](docs/BUILDING.md)에 있습니다.

## 원본 프로젝트와 라이선스

NestView는 [qView](https://github.com/jurplel/qView) 7.0을 기반으로 UI와 탐색 기능을 확장한 프로젝트입니다. 원본 qView 개발자와 기여자들의 작업에 감사드립니다.

기존 **GNU GPL v3** 라이선스를 유지합니다. [LICENSE](LICENSE)와 [서드파티 고지](docs/THIRD_PARTY.md)를 참고하세요.
