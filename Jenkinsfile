#!groovy

//TODO: store docker image in registry?
//dockerImageName="dockerregistry.macio.de:5000/macio_backup_archiver"
dockerImageName='macio_backup_archiver'

// build
def build()
{
  try
  {
    stage("Docker image")
    {
      checkout scm
      sh "(cd docker; docker build -t ${dockerImageName} .)"
    }


    stage("Build")
    {
      docker.image(dockerImageName).inside
      {
        checkout scm
        sh "./download-third-party-packages.sh"
        sh "./configure"
//TODO
//        sh "make -j`nproc --ignore=1`"
        sh "make"
        sh "make dist"
      }
    }

    stage("Packages")
    {
      docker.image(dockerImageName).inside
      {
//TODO: how to run a docker from inside a docker?
//        sh "make update_docker"
//        sh "make rpm"
//        sh "make deb"
        sh "make gui"
      }
    }

    stage("Tests")
    {
      script
      {
        if (params.TESTS)
        {
          switch (params.BINARY_TYPE)
          {
            case "DEBUG":
              binaryExtension="-debug";
              break;
            case "VALGRIND":
              binaryExtension="-valgrind";
              break;
            case "GCOV":
              binaryExtension="-gcov";
              break;
            case "GPROF":
              binaryExtension="-gprof";
              break;
            default:
              binaryExtension="";
              break;
          }
          sh "echo binaryExtension="+binaryExtension
          switch (params.TEST_TYPE)
          {
            case "SMOKE":
            default:
              docker.image(dockerImageName).inside
              {
                sh "make test_smoke-"+binaryExtension+" O='--verbose=1'"
              }
              break;
            case "FULL":
              docker.image(dockerImageName).inside
              {
                sh "make test1"+binaryExtension+" O='--verbose=1'"
                sh "make test2"+binaryExtension+" O='--verbose=1'"
                sh "make test3"+binaryExtension+" O='--verbose=1'"
                sh "make test4"+binaryExtension+" O='--verbose=1'"
                sh "make test5"+binaryExtension+" O='--verbose=1'"
                sh "make test6"+binaryExtension+" O='--verbose=1'"
                sh "make test7"+binaryExtension+" O='--verbose=1'"
              }
              break;
          }
        }
      }
    }

    stage("Code coverage")
    {
      script
      {
        if (params.CODE_COVERAGE)
        {
          docker.image(dockerImageName).inside
          {
            sh "make gcov_clean"
            sh "make gcov1"
            sh "make gcov2"
            sh "make gcov3"
            sh "make gcov4"
            sh "make gcov5"
            sh "make gcov6"
            sh "make gcov7"
          }
        }
      }

      cobertura coberturaReportFile: 'coverage.xml', enableNewApi: true, lineCoverageTargets: '80, 60, 70'
    }

    stage("Profiling")
    {
      script
      {
        if (params.PROFILING)
        {
          docker.image(dockerImageName).inside
          {
            sh "make gprof1"
            sh "make gprof2"
            sh "make gprof3"
            sh "make gprof4"
            sh "make gprof5"
            sh "make gprof6"
            sh "make gprof7"
          }
        }
      }
    }
  }
  finally
  {
    archiveArtifacts artifacts: 'backup-archiver*.tar.bz2, *.rpm, *.deb, *.zip coverage/*', allowEmptyArchive: 'true'
    cleanWs notFailBuild: true
  }
}

// main
properties([gitLabConnection('GitLab-DMZ')])
try
{
  node('docker')
  {
    build()
  }
}
catch (error)
{
  echo "ERROR: ${error.message}"
  currentBuild.result = 'FAILED'
}
