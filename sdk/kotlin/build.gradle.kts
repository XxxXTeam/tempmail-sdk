plugins {
    kotlin("jvm") version "2.0.21"
    kotlin("plugin.serialization") version "2.0.21"
    id("com.vanniktech.maven.publish") version "0.34.0"
}

group = "io.github.xxxxteam"
version = "0.1.0"

repositories {
    mavenCentral()
}

val ktorVersion = "3.0.3"

dependencies {
    implementation("io.ktor:ktor-client-core:$ktorVersion")
    implementation("io.ktor:ktor-client-cio:$ktorVersion")
    implementation("io.ktor:ktor-client-content-negotiation:$ktorVersion")
    implementation("io.ktor:ktor-client-websockets:$ktorVersion")
    implementation("io.ktor:ktor-serialization-kotlinx-json:$ktorVersion")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3")
    implementation(kotlin("reflect"))

    testImplementation(kotlin("test"))
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
}

kotlin {
    jvmToolchain(17)
}

tasks.test {
    useJUnitPlatform()
}

// 发布到 Maven Central（Sonatype Central Portal）。
// 凭据与 GPG 私钥通过环境变量注入（ORG_GRADLE_PROJECT_ 前缀）：
//   ORG_GRADLE_PROJECT_mavenCentralUsername / mavenCentralPassword —— Central Portal User Token
//   ORG_GRADLE_PROJECT_signingInMemoryKey / signingInMemoryKeyPassword —— ASCII armored GPG 私钥与密码
mavenPublishing {
    publishToMavenCentral()
    signAllPublications()

    coordinates(group.toString(), "tempmail-sdk", version.toString())

    pom {
        name.set("TempMail SDK")
        description.set("Kotlin SDK for temporary email services, aggregating 279 channels")
        inceptionYear.set("2026")
        url.set("https://github.com/XxxXTeam/tempmail-sdk")
        licenses {
            license {
                name.set("GNU General Public License v3.0")
                url.set("https://www.gnu.org/licenses/gpl-3.0.txt")
            }
        }
        developers {
            developer {
                id.set("xxxxteam")
                name.set("XxxXTeam")
                url.set("https://github.com/XxxXTeam")
            }
        }
        scm {
            url.set("https://github.com/XxxXTeam/tempmail-sdk")
            connection.set("scm:git:git://github.com/XxxXTeam/tempmail-sdk.git")
            developerConnection.set("scm:git:ssh://git@github.com/XxxXTeam/tempmail-sdk.git")
        }
    }
}
